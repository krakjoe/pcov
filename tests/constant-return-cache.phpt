--TEST--
constant-return coverage survives file caching and isolates PCOV enabled and disabled opcodes
--SKIPIF--
<?php
if (!extension_loaded('pcov') || !extension_loaded('Zend OPcache')) {
    print 'skip extensions unavailable';
} elseif (!function_exists('proc_open') || !function_exists('opcache_is_script_cached_in_file_cache')) {
    print 'skip required process or file-cache API unavailable';
}
?>
--EXTENSIONS--
opcache
--INI--
pcov.enabled=1
--FILE--
<?php
/** Locate the tested shared extension, including out-of-tree builds. */
function extensionPath(string $name): ?string
{
    $suffix = PHP_OS_FAMILY === 'Windows' ? '.dll' : '.so';
    foreach ([
        dirname(__DIR__) . "/modules/$name$suffix",
        rtrim(ini_get('extension_dir'), DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . "$name$suffix",
    ] as $candidate) {
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

/** Run a fresh process with an optional environment override of pcov.enabled. */
function runWorker(string $cache, bool $iniEnabled, ?string $override): array
{
    $pcov = extensionPath('pcov');
    if (!$pcov) {
        throw new RuntimeException('cannot locate pcov shared library');
    }
    $command = [PHP_BINARY, '-n'];
    if ($opcache = extensionPath('opcache')) {
        array_push($command, '-d', "zend_extension=$opcache");
    }
    array_push(
        $command,
        '-d', "extension=$pcov",
        '-d', 'pcov.enabled=' . (int) $iniEnabled,
        '-d', 'pcov.directory=' . __DIR__,
        '-d', 'opcache.enable_cli=1',
        '-d', 'opcache.jit=disable',
        '-d', 'opcache.jit_buffer_size=0',
        '-d', 'opcache.optimization_level=-1',
        '-d', "opcache.file_cache=$cache",
        '-d', 'opcache.file_update_protection=0',
        __DIR__ . '/jit/cache-identity-worker.php',
    );
    $environment = getenv();
    unset($environment['PCOV_ENABLED']);
    if ($override !== null) {
        $environment['PCOV_ENABLED'] = $override;
    }
    $pipes = [];
    $process = proc_open($command, [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes, null, $environment);
    $stdout = stream_get_contents($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    $status = proc_close($process);
    if ($status !== 0 || $stderr !== '') {
        throw new RuntimeException("worker failed ($status): $stderr$stdout");
    }
    return json_decode($stdout, true, flags: JSON_THROW_ON_ERROR);
}

/** Remove only the temporary cache created by this test. */
function removeTree(string $directory): void
{
    foreach (new FilesystemIterator($directory) as $entry) {
        if ($entry->isDir() && !$entry->isLink()) {
            removeTree($entry->getPathname());
        } else {
            unlink($entry->getPathname());
        }
    }
    rmdir($directory);
}

$cache = sys_get_temp_dir() . '/pcov-constant-return-cache-' . bin2hex(random_bytes(8));
mkdir($cache);
try {
    foreach ([
        [false, null, false, false],
        [false, '1', true, false],
        [true, null, true, true],
        [true, '0', false, true],
    ] as [$iniEnabled, $override, $expectedEnabled, $expectedCached]) {
        $actual = runWorker($cache, $iniEnabled, $override);
        $expected = [
            'cached' => $expectedCached,
            'enabled' => $expectedEnabled,
            'optimization' => $expectedEnabled ? (-1 & ~0x8000) : -1,
            'hits' => $expectedEnabled ? [true, true, false, false] : null,
        ];
        if ($actual !== $expected) {
            throw new RuntimeException('unexpected worker result: ' . json_encode($actual));
        }
    }
} finally {
    removeTree($cache);
}
echo "OK\n";
?>
--EXPECT--
OK
