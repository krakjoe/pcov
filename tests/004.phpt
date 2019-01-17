--TEST--
\pcov\collect not recognized filter type
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 1
--FILE--
<?php
var_dump(\pcov\collect(42));
?>
--EXPECTF--
Fatal error: Uncaught TypeError: type must be \pcov\inclusive, \pcov\exclusive, or \pcov\all in %s/004.php:2
Stack trace:
#0 %s/004.php(2): pcov\collect(42)
#1 {main}
  thrown in %s/004.php on line 2

