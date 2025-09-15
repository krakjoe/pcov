--TEST--
enable/disable (enable env as on)
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 0
--ENV--
PCOV_ENABLED=on
--FILE--
<?php
var_dump(\getenv("PCOV_ENABLED"));
var_dump(\pcov\enabled());
?>
--EXPECT--
string(2) "on"
bool(true)
