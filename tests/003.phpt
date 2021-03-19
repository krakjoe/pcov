--TEST--
\pcov\collect non array filter
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 0
--FILE--
<?php
var_dump(\pcov\collect(\pcov\inclusive, ""));
?>
--EXPECTF--
Fatal error: Uncaught TypeError: %s type array, string given in %s%e003.php:2
Stack trace:
#0 %s%e003.php(2): pcov\collect(1, '')
#1 {main}
  thrown in %s%e003.php on line 2
