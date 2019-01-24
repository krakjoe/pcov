--TEST--
clear
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 1
--FILE--
<?php
\pcov\start();
$d = [];
for ($i = 0; $i < 10; $i++) {
	$d[] = $i * 42;
}
\pcov\stop();

var_dump(\pcov\collect());

\pcov\clear();

var_dump(\pcov\collect());
?>
--EXPECTF--
array(1) {
  ["%s%e002.php"]=>
  array(9) {
    [2]=>
    int(-1)
    [3]=>
    int(1)
    [4]=>
    int(1)
    [5]=>
    int(1)
    [7]=>
    int(1)
    [9]=>
    int(-1)
    [11]=>
    int(-1)
    [13]=>
    int(-1)
    [15]=>
    int(-1)
  }
}
array(0) {
}


