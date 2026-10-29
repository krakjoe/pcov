<?php

$target = __DIR__ . '/coverage-target.php';
$jit = opcache_get_status(false)['jit'] ?? null;

if (($expectedFileCache ?? false) && !opcache_is_script_cached_in_file_cache($target)) {
    throw new RuntimeException('target was not loaded from file cache');
}

if ($expectedJit === 'off') {
    if ($jit && $jit['on']) {
        throw new RuntimeException('JIT unexpectedly enabled');
    }
} elseif (!$jit || !$jit['on'] || ($expectedJit === 'function' && $jit['kind'] !== 0) || ($expectedJit === 'tracing' && $jit['kind'] !== 5)) {
    throw new RuntimeException("$expectedJit JIT is not active");
}

\pcov\stop();
require $target;
$schema = new PcovJitSchema();

for ($run = 0; $run < 500; $run++) {
    pcov_jit_implicit_return($run);
    pcov_jit_ternary(($run % 2) === 0);
    pcov_jit_loop([-1, $run, 2]);
    pcov_jit_match(($run % 3) + 1);
    pcov_jit_exception(($run % 2) === 0);
    iterator_to_array(pcov_jit_generator());
    pcov_jit_recursive(3);
    pcov_jit_side_exit($run);
    $schema->describe(($run % 2) === 0);
    $schema->stoppedSchema();
}

\pcov\clear();
\pcov\start();
pcov_jit_implicit_return(1);
pcov_jit_ternary(true);
pcov_jit_ternary(false);
pcov_jit_loop([-1, 2, 3]);
pcov_jit_match(1);
pcov_jit_match(2);
pcov_jit_match(3);
pcov_jit_exception(false);
pcov_jit_exception(true);
iterator_to_array(pcov_jit_generator());
pcov_jit_recursive(3);
pcov_jit_side_exit(1);
pcov_jit_side_exit('side-exit');
$schema->describe(false);
$schema->describe(true);
\pcov\stop();
pcov_jit_stopped_only();
$schema->stoppedSchema();

$first = \pcov\collect(\pcov\inclusive, [realpath($target)]);

/** Assert actual method hits, since JIT parity can otherwise preserve a shared coverage bug. */
function assertSchemaCoverage(array $lines, bool $called): void
{
    foreach (['banSchema' => $called, 'mutationSchema' => $called, 'unusedSchema' => false, 'stoppedSchema' => false] as $name => $expected) {
        $method = new ReflectionMethod(PcovJitSchema::class, $name);
        $covered = false;
        foreach ($lines as $line => $hit) {
            if ($line >= $method->getStartLine() && $line <= $method->getEndLine() && $hit > 0) {
                $covered = true;
            }
        }
        if ($covered !== $expected) {
            throw new RuntimeException("Unexpected coverage for $name: expected " . (int) $expected);
        }
    }
}

assertSchemaCoverage($first[realpath($target)], true);

\pcov\clear();
\pcov\start();
pcov_jit_implicit_return(2);
\pcov\stop();

$second = \pcov\collect(\pcov\inclusive, [realpath($target)]);
assertSchemaCoverage($second[realpath($target)], false);

echo json_encode([
    'first' => $first[realpath($target)],
    'after_clear' => $second[realpath($target)],
], JSON_PRETTY_PRINT), "\n";
