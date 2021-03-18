--TEST--
anonymous class
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 1
--FILE--
<?php
\pcov\start();
$a = new class() {};
\pcov\stop();
var_dump(\pcov\collect());
?>
--EXPECTF--
array(1) {
  ["%s%e005.php"]=>
  array(5) {
    [2]=>
    int(-1)
    [3]=>
    int(1)
    [4]=>
    int(1)
    [5]=>
    int(-1)
    [7]=>
    int(-1)
  }
}
