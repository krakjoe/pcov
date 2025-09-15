--TEST--
enable/disable (enable env as yes)
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 0
--ENV--
PCOV_ENABLED=yes
--FILE--
<?php
var_dump(\getenv("PCOV_ENABLED"));
var_dump(\pcov\enabled());
?>
--EXPECT--
string(3) "yes"
bool(true)
