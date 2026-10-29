--TEST--
FPM coverage is stable across JIT modes
--SKIPIF--
<?php
if (PHP_OS_FAMILY === 'Windows') {
    print 'skip FPM unavailable on Windows';
} elseif (!extension_loaded('pcov') || !extension_loaded('Zend OPcache')) {
    print 'skip extensions unavailable';
} else {
    $prefix = dirname(dirname(PHP_BINARY));
    $candidates = array_filter([
        getenv('TEST_PHP_FPM_EXECUTABLE'),
        "$prefix/sbin/php-fpm",
        dirname(PHP_BINARY) . '/php-fpm',
    ]);
    if (!array_find($candidates, fn(string $path): bool => is_executable($path))) {
        print 'skip TEST_PHP_FPM_EXECUTABLE is not available';
    }
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
    $candidates = [
        dirname(__DIR__) . "/modules/$name.so",
        rtrim(ini_get('extension_dir'), DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . "$name.so",
    ];

    foreach ($candidates as $candidate) {
        if (is_file($candidate)) {
            return $candidate;
        }
    }

    if (is_readable('/proc/self/maps')) {
        foreach (file('/proc/self/maps', FILE_IGNORE_NEW_LINES) as $mapping) {
            if (preg_match('~\s(/\S*/' . preg_quote($name, '~') . '\.so(?:\.\S*)?)$~', $mapping, $matches)) {
                return $matches[1];
            }
        }
    }

    return null;
}

function fastcgiLength(int $length): string
{
    return $length < 128 ? chr($length) : pack('N', $length | 0x80000000);
}

function fastcgiRecord(int $type, string $content): string
{
    $padding = (8 - (strlen($content) % 8)) % 8;
    return pack('CCnnCC', 1, $type, 1, strlen($content), $padding, 0)
        . $content
        . str_repeat("\0", $padding);
}

function fastcgiRequest(string $socket, string $script, int $request = 0): string
{
    $stream = stream_socket_client("unix://$socket", $errorCode, $errorMessage, 5);
    if (!$stream) {
        throw new RuntimeException("FastCGI connection failed: $errorMessage ($errorCode)");
    }

    $params = [
        'GATEWAY_INTERFACE' => 'CGI/1.1',
        'SERVER_PROTOCOL' => 'HTTP/1.1',
        'REQUEST_METHOD' => 'GET',
        'SCRIPT_FILENAME' => $script,
        'SCRIPT_NAME' => '/' . basename($script),
        'DOCUMENT_ROOT' => dirname($script),
        'QUERY_STRING' => (string) $request,
        'REMOTE_ADDR' => '127.0.0.1',
        'REMOTE_PORT' => '12345',
        'SERVER_ADDR' => '127.0.0.1',
        'SERVER_PORT' => '80',
        'SERVER_NAME' => 'localhost',
    ];
    $encoded = '';
    foreach ($params as $name => $value) {
        $encoded .= fastcgiLength(strlen($name)) . fastcgiLength(strlen($value)) . $name . $value;
    }

    fwrite($stream, fastcgiRecord(1, pack('nCxxxxx', 1, 0)));
    fwrite($stream, fastcgiRecord(4, $encoded));
    fwrite($stream, fastcgiRecord(4, ''));
    fwrite($stream, fastcgiRecord(5, ''));

    $stdout = '';
    $stderr = '';
    $ended = false;
    while (!feof($stream)) {
        $header = fread($stream, 8);
        if (strlen($header) !== 8) {
            break;
        }
        $record = unpack('Cversion/Ctype/nrequest/nlength/Cpadding/Creserved', $header);
        $content = $record['length'] ? stream_get_contents($stream, $record['length']) : '';
        if ($record['padding']) {
            stream_get_contents($stream, $record['padding']);
        }
        if ($record['type'] === 6) {
            $stdout .= $content;
        } elseif ($record['type'] === 7) {
            $stderr .= $content;
        } elseif ($record['type'] === 3) {
            $ended = true;
            break;
        }
    }
    fclose($stream);

    if ($stderr !== '') {
        throw new RuntimeException($stderr);
    }
    if (!$ended) {
        throw new RuntimeException('FPM closed the connection without END_REQUEST');
    }
    $parts = preg_split("~\r?\n\r?\n~", $stdout, 2);
    return $parts[1] ?? '';
}

function runFpm(string $fpm, string $mode, string $root, bool $recycle = false): string
{
    $socket = "$root/fpm.sock";
    $config = "$root/fpm.conf";
    $errorLog = "$root/error.log";
    $maxRequests = $recycle ? 1 : 0;
    file_put_contents($config, <<<INI
[global]
daemonize = no
error_log = $errorLog
[www]
listen = $socket
pm = static
pm.max_children = 1
pm.max_requests = $maxRequests
clear_env = no
catch_workers_output = yes
env[PCOV_EXPECTED_JIT] = "$mode"
INI);

    $pcov = extensionPath('pcov');
    $opcache = extensionPath('opcache');
    if (!$pcov) {
        throw new RuntimeException('cannot locate pcov shared library');
    }

    $command = [$fpm, '-n', '-F', '-R', '-y', $config];
    if ($opcache) {
        array_push($command, '-d', "zend_extension=$opcache");
    }
    array_push(
        $command,
        '-d', "extension=$pcov",
        '-d', 'pcov.enabled=1',
        '-d', 'pcov.directory=' . __DIR__,
        '-d', 'opcache.enable=1',
        '-d', 'opcache.file_update_protection=0',
        '-d', 'opcache.jit_buffer_size=' . ($mode === 'off' ? '0' : '16M'),
        '-d', "opcache.jit=$mode",
        '-d', 'opcache.jit_hot_func=1',
        '-d', 'opcache.jit_hot_loop=1',
        '-d', 'opcache.jit_hot_return=1',
        '-d', 'opcache.jit_hot_side_exit=1',
        '-d', 'opcache.optimization_level=-1',
    );

    $pipes = [];
    $process = proc_open($command, [['pipe', 'r'], ['pipe', 'w'], ['pipe', 'w']], $pipes);
    fclose($pipes[0]);

    try {
        $deadline = microtime(true) + 5;
        while (!file_exists($socket) && microtime(true) < $deadline) {
            if (!proc_get_status($process)['running']) {
                throw new RuntimeException('FPM exited during startup: ' . stream_get_contents($pipes[2]));
            }
            usleep(10000);
        }
        if (!file_exists($socket)) {
            throw new RuntimeException('FPM socket was not created');
        }
        $worker = __DIR__ . '/jit/' . ($recycle ? 'fpm-lifetime-worker.php' : 'fpm-worker.php');
        $first = fastcgiRequest($socket, $worker);
        for ($request = 1; $request < ($recycle ? 12 : 2); $request++) {
            if (fastcgiRequest($socket, $worker, $request) !== $first) {
                throw new RuntimeException("$mode FPM coverage changed on request $request (recycle=$maxRequests)");
            }
        }
        return $first;
    } catch (Throwable $exception) {
        throw new RuntimeException($exception->getMessage() . "\n" . (is_file($errorLog) ? file_get_contents($errorLog) : ''), 0, $exception);
    } finally {
        proc_terminate($process);
        fclose($pipes[1]);
        fclose($pipes[2]);
        proc_close($process);
    }
}

$prefix = dirname(dirname(PHP_BINARY));
$fpm = array_find(array_filter([
    getenv('TEST_PHP_FPM_EXECUTABLE'),
    "$prefix/sbin/php-fpm",
    dirname(PHP_BINARY) . '/php-fpm',
]), fn(string $path): bool => is_executable($path));
$root = sys_get_temp_dir() . '/pcov-jit-fpm-' . bin2hex(random_bytes(8));
mkdir($root);

try {
    foreach ([false, true] as $recycle) {
        $reference = null;
        foreach (['off', 'function', 'tracing'] as $mode) {
            $modeRoot = "$root/$mode-" . (int) $recycle;
            mkdir($modeRoot);
            $coverage = runFpm($fpm, $mode, $modeRoot, $recycle);
            if ($reference !== null && $reference !== $coverage) {
                throw new RuntimeException("$mode FPM coverage differs");
            }
            $reference = $coverage;
        }
    }
} finally {
    foreach (new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator($root, FilesystemIterator::SKIP_DOTS),
        RecursiveIteratorIterator::CHILD_FIRST,
    ) as $entry) {
        $entry->isDir() ? rmdir($entry->getPathname()) : unlink($entry->getPathname());
    }
    rmdir($root);
}

echo "OK\n";
?>
--EXPECT--
OK
