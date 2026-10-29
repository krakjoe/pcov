<?php

// Alternate heap layouts before creating closures, then retire the whole worker.
// Shared traces must not retain any of these workers' private op_array copies.
$heap = [];
for ($i = 0; $i < ((int) ($_SERVER['QUERY_STRING'] ?? 0) % 2 ? 100 : 20000); $i++) {
    $heap[] = static fn (): int => 1;
}

$target = __DIR__ . '/fpm-lifetime-target.php';
require $target;

// A request-defined parent prevents OPcache from persisting the linked child.
// Trait methods are copied at binding time, after the heap layout has changed.
eval('class PcovLifetimeRuntimeParent {}');
class PcovLifetimeConsumer extends PcovLifetimeRuntimeParent
{
    use PcovLifetimeTrait;
}

\pcov\start();
$consumer = new PcovLifetimeConsumer();
for ($generation = 0; $generation < 8; $generation++) {
    $callback = pcov_lifetime_factory($generation);
    for ($i = 0; $i < 1000; $i++) {
        if (pcov_lifetime_call($callback, $i) !== $i + $generation
            || $consumer->increment($i) !== $i + 1) {
            throw new RuntimeException('Incorrect JIT result');
        }
    }
    unset($callback);
    gc_mem_caches();
}
\pcov\stop();
$coverage = \pcov\collect(\pcov\inclusive, [$target])[$target] ?? [];
foreach (file($target) as $line => $source) {
    if (str_contains($source, '// covered') && ($coverage[$line + 1] ?? null) !== 1) {
        throw new RuntimeException('Missing coverage on line ' . ($line + 1));
    }
    if (str_contains($source, '// uncovered') && ($coverage[$line + 1] ?? null) !== -1) {
        throw new RuntimeException('Unexpected coverage on line ' . ($line + 1));
    }
}
$jit = opcache_get_status(false)['jit'];
if (getenv('PCOV_EXPECTED_JIT') !== 'off' && (!$jit['on'] || $jit['buffer_free'] >= $jit['buffer_size'] - 4096)) {
    throw new RuntimeException('The lifetime regression must execute JIT code');
}
ksort($coverage);
echo json_encode($coverage, JSON_THROW_ON_ERROR), "\n";
