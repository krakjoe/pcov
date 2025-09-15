--TEST--
enable/disable (disable env as no)
--SKIPIF--
<?php if (!extension_loaded("pcov")) print "skip"; ?>
--INI--
pcov.enabled = 1
--ENV--
PCOV_ENABLED=no
--FILE--
<?php
var_dump(\getenv("PCOV_ENABLED"));
var_dump(\pcov\enabled());
?>
--EXPECT--
string(2) "no"
bool(false)
