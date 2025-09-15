--TEST--
enable/disable (enable env as 1)
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 0
--ENV--
PCOV_ENABLED=1
--FILE--
<?php
var_dump(\getenv("PCOV_ENABLED"));
var_dump(\pcov\enabled());
?>
--EXPECT--
string(1) "1"
bool(true)
