--TEST--
enable/disable (disable env as 0)
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 1
--ENV--
PCOV_ENABLED=0
--FILE--
<?php
var_dump(\getenv("PCOV_ENABLED"));
var_dump(\pcov\enabled());
?>
--EXPECT--
string(1) "0"
bool(false)
