--TEST--
stack limit check is preserved when pcov overrides the executor
--SKIPIF--
<?php
if (!extension_loaded("pcov")) print "skip";
if (PHP_VERSION_ID < 80300) print "skip requires PHP 8.3+ (zend.max_allowed_stack_size)";
?>
--INI--
pcov.enabled = 1
zend.max_allowed_stack_size = 1048576
--FILE--
<?php
function pcov_recurse(array $items): int {
	return array_reduce($items, fn($carry, $item) => pcov_recurse([$item]), 0);
}

try {
	pcov_recurse([1]);
	echo "no error\n";
} catch (\Error $e) {
	printf("%s: %s\n", get_class($e), $e->getMessage());
}
?>
--EXPECTF--
Error: Maximum call stack size of %d bytes (zend.max_allowed_stack_size - zend.reserved_stack_size) reached. Infinite recursion?
