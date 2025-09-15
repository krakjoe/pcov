--TEST--
enable/disable (disable env as off)
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 1
--ENV--
PCOV_ENABLED=off
--FILE--
<?php
var_dump(\getenv("PCOV_ENABLED"));
var_dump(\pcov\enabled());
?>
--EXPECT--
string(3) "off"
bool(false)
