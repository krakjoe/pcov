--TEST--
Property hooks
--SKIPIF--
<?php
if (!extension_loaded("pcov")) print "skip";
if (PHP_VERSION_ID < 80400) print "skip only for PHP >= 8.4";
?>
--INI--
pcov.enabled = 1
--FILE--
<?php
\pcov\start();
class MyClass {
  public int $myGetOnlyProp {
    get => 1;
  }

  public float $myGetSetProp {
    get {
      $tmp = $this->myGetSetProp;
      return $tmp * 2;
    }

    set(float $value) {
      $this->myGetSetProp = $value / 2;
    }
  }
}
$instance = new MyClass;
$instance->myGetOnlyProp;
$instance->myGetSetProp = 1;
$instance->myGetSetProp;
\pcov\stop();
var_dump(\pcov\collect());
?>
--EXPECTF--
array(1) {
  ["%s%ehooks.php"]=>
  array(13) {
    [2]=>
    int(-1)
    [19]=>
    int(1)
    [20]=>
    int(1)
    [21]=>
    int(1)
    [22]=>
    int(1)
    [23]=>
    int(1)
    [24]=>
    int(-1)
    [26]=>
    int(-1)
    [5]=>
    int(1)
    [10]=>
    int(1)
    [11]=>
    int(1)
    [15]=>
    int(1)
    [16]=>
    int(1)
  }
}
