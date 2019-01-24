--TEST--
start/stop
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
?>
--EXPECTF--
array(1) {
  ["%s%e001.php"]=>
  array(7) {
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
  }
}

