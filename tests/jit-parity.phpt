--TEST--
coverage is byte-identical with JIT disabled, function JIT, and tracing JIT
--SKIPIF--
<?php
if (!extension_loaded('pcov') || !extension_loaded('Zend OPcache')) {
    print 'skip extensions unavailable';
} elseif (!function_exists('proc_open')) {
    print 'skip proc_open unavailable';
}
?>
--EXTENSIONS--
opcache
--INI--
pcov.enabled=1
--FILE--
<?php
function extensionPath(string $name): ?string
{
    $suffix = PHP_OS_FAMILY === 'Windows' ? '.dll' : '.so';
    $candidates = [
        dirname(__DIR__) . '/modules/' . $name . $suffix,
        rtrim(ini_get('extension_dir'), DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $name . $suffix,
    ];

    foreach ($candidates as $candidate) {
        if (is_file($candidate)) {
            return $candidate;
        }
    }

    if (PHP_OS_FAMILY === 'Linux' && is_readable('/proc/self/maps')) {
        foreach (file('/proc/self/maps', FILE_IGNORE_NEW_LINES) as $mapping) {
            if (preg_match('~\s(/\S*/' . preg_quote($name, '~') . '\.so(?:\.\S*)?)$~', $mapping, $matches)) {
                return $matches[1];
            }
        }
    }

    return null;
}

function runWorker(string $mode, string $optimization): string
{
    $pcov = extensionPath('pcov');
    $opcache = extensionPath('opcache');
    if (!$pcov) {
        throw new RuntimeException('cannot locate pcov shared library');
    }

    $command = [PHP_BINARY, '-n'];
    if ($opcache) {
        array_push($command, '-d', "zend_extension=$opcache");
    }
    array_push(
        $command,
        '-d', "extension=$pcov",
        '-d', 'pcov.enabled=1',
        '-d', 'pcov.directory=' . __DIR__,
        '-d', 'opcache.enable_cli=1',
        '-d', 'opcache.jit_buffer_size=' . ($mode === 'off' ? '0' : '16M'),
        '-d', "opcache.jit=$mode",
        '-d', 'opcache.jit_hot_func=1',
        '-d', 'opcache.jit_hot_loop=1',
        '-d', 'opcache.jit_hot_return=1',
        '-d', 'opcache.jit_hot_side_exit=1',
        '-d', "opcache.optimization_level=$optimization",
        __DIR__ . '/jit/file-cache-worker.php',
        $mode,
        0,
    );

    $pipes = [];
    $process = proc_open($command, [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    $stdout = stream_get_contents($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    $status = proc_close($process);
    if ($status !== 0) {
        throw new RuntimeException("worker failed ($status): $stderr$stdout");
    }
    return $stdout;
}

foreach (['0', '-1'] as $optimization) {
    $reference = runWorker('off', $optimization);
    foreach (['function', 'tracing'] as $mode) {
        if (runWorker($mode, $optimization) !== $reference) {
            throw new RuntimeException("$mode JIT coverage differs at optimization level $optimization");
        }
    }
}

echo "OK\n";
?>
--EXPECT--
OK
