--TEST--
anonymous function
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 1
--FILE--
<?php
\pcov\start();
$a = function() {
    return 'a';
};
$a();
\pcov\stop();
var_dump(\pcov\collect());
?>
--EXPECTF--
array(1) {
  ["%s%e007.php"]=>
  array(8) {
    [2]=>
    int(-1)
    [3]=>
    int(1)
    [5]=>
    int(1)
    [6]=>
    int(1)
    [7]=>
    int(1)
    [8]=>
    int(-1)
    [10]=>
    int(-1)
    [4]=>
    int(1)
  }
}
