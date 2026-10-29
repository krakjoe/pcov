<?php

$target = __DIR__ . '/coverage-target.php';
$cached = opcache_is_script_cached_in_file_cache($target);
require $target;

$enabled = \pcov\enabled();
$hits = null;
if ($enabled) {
    \pcov\start();
    $schema = new PcovJitSchema();
    if ($schema->describe(false) !== ['ban' => 'string']
        || $schema->describe(true) !== ['success' => 'bool']) {
        throw new RuntimeException('schema values changed');
    }
    \pcov\stop();
    $lines = \pcov\collect(\pcov\inclusive, [$target])[$target];
    $hits = [];
    foreach (['banSchema', 'mutationSchema', 'unusedSchema', 'stoppedSchema'] as $name) {
        $method = new ReflectionMethod(PcovJitSchema::class, $name);
        $covered = false;
        foreach ($lines as $line => $hit) {
            if ($line >= $method->getStartLine() && $line <= $method->getEndLine() && $hit > 0) {
                $covered = true;
            }
        }
        $hits[] = $covered;
    }
}

echo json_encode([
    'cached' => $cached,
    'enabled' => $enabled,
    'optimization' => opcache_get_configuration()['directives']['opcache.optimization_level'],
    'hits' => $hits,
]), "\n";
