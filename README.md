PCOV
====

[![Build Status](https://travis-ci.org/krakjoe/pcov.svg?branch=develop)](https://travis-ci.org/krakjoe/pcov)

A self contained [CodeCoverage](https://github.com/sebastianbergmann/php-code-coverage) compatible driver for PHP7

Requirements
============

  * PHP 7
  * A care free attitude to life

API
===

```php
/*
* Shall start recording coverage information
**/
function \pcov\start(void) : void;

/*
* Shall stop recording coverage information
**/
function \pcov\stop(void) : void;

/*
* Shall collect coverage information
* @param integer \pcov\all        shall collect coverage information for all files
*		 \pcov\inclusive  shall collect coverage information for the specified files
*		 \pcov\exclusive  shall collect coverage information for all but the specified files
* @param array   filenames
* Note: paths in filter must be realpath
*/
function \pcov\collect(int $type = \pcov\all, array $filter = []) : array;

/*
* Shall clear stored coverage information
* @param set true to clear file cache
* Note: clearing the file tables may have surprising consequences
*	the most effective strategy is to use stop and start precisely
*	and leave pcov to clear on shutdown
*/
function \pcov\clear(bool $files = false) : void;

/*
* Shall return a list of included files
* Note: after the first invocation will return newly included files only
*/
function \pcov\includes() : array;
```

Configuration
=============

  * pcov.enabled          (default: 1, system)                  shall enable or disable zend hooks for pcov
  * pcov.directory        (default: auto, system,dir)           shall restrict collection to a single directory
  * pcov.initial.memory   (default: 65536, system,dir)          shall set the initial size of the arena used by pcov
  * pcov.initial.files    (default: 64, system,dir)             shall set the initial size of the files table

The recommended defaults for production should be:

  * pcov.enabled = 0

The recommended defaults for development should be:

  * pcov.enabled = 1
  * pcov.directory = /path/to/your/source/tree

When ```pcov.directory``` is left unset, pcov will attempt to find ```src```, ```lib``` or, ```app``` in the current
working directory, in that order; If none are found the current directory will be used, which may waste resources storing
coverage information for your test suite.

Interoperability
================

When pcov is enabled by configuration ```pcov.enabled=1```:

  * interoperability with xdebug is not possible
  * interoperability with phpdbg is not possible

At an internals level, the executor function is overriden by pcov, so any extension or SAPI which does the same will be broken.

When pcov is disabled by configuration ```pcov.enabled=0```:

  * pcov is zero cost - code runs at full speed
  * xdebug may be loaded
  * phpdbg may be executed

At an internals level, the executor function is untouched, and pcov allocates no memory.

Rationale
=========

It is noticable that pcov does not generate precisely the same report as xdebug or phpdbg, which requires a rationale.

All code coverage implementations must be opinionated about the instructions that are included in the report: All of them
must omit some opcodes so that their reports make sense to the end user - the end user is not interested in how many NOP's
or TICK's are in their code.

xdebug takes a lightly footed approach, only counting a few opcodes as implementation details that are not important to the
end user and so ignoring them. Noteworthy that xdebug has backward compatibility concerns unlike almost any other PHP 
extension, it's user base is vast and slight adjustments to how it may work, however well justified may cause friction 
among that established user base.

phpdbg takes a slightly more heavy handed approach, but a justified one: It attempts to ignore opcodes where their presence
or execution may depend on configuration; The presence of opcache, or it's optimization level for example. It also counts a 
few more opcodes as implementation details. This isn't justified in theory but in practice - it ignores the trait opcodes 
because they are an implementation detail and indeed these opcodes have been removed in the master branch of PHP (7.4).

xdebug ignores the following opcodes:

  * `ZEND_NOP`                   - non instruction, ignored for obvious reasons
  * `ZEND_EXT_NOP`               - non instruction
  * `ZEND_OP_DATA`               - non instruction
  * `ZEND_RECV_*`                - function prologue
  * `ZEND_ADD_INTERFACE`         - ze implementation detail
  * `ZEND_VERIFY_ABSTRACT_CLASS` - ze implementation detail
  * `ZEND_TICKS`                 - presence depends on configuration

phpdbg ignores the following additional opcodes:

  * `ZEND_FREE`                  - ze implementation detail
  * `ZEND_FE_FREE`               - ze implementation detail
  * `ZEND_ASSERT_CHECK`          - execution depends on configuration
  * `ZEND_VERIFY_RETURN_TYPE`    - ze implementation detail - will only ever be on the same line as a ZEND_RETURN instruction
  * `ZEND_DECLARE_CONST`         - ze implementation detail - may not be executed, may be optimized away
  * `ZEND_DECLARE_*`             - ze implementation details - may not be executed, may be optimized away
  * `ZEND_FAST_RET`              - ze implementation detail - only ever on lines with jumps/calls
  * `ZEND_EXT_*`                 - ze extension implementation details, presence depends on configuration
  * `ZEND_*_TRAITS`              - ze implementation detail
  * `ZEND_BIND_GLOBAL`           - ze implementation detail

pcov ignores the following additional opcodes:

  * `ZEND_FAST_CALL`             - ze implementation detail, also ignored by xdebug
  * `ZEND_SEND_*`		 - function calling convention - will always be on the same line as the following call instruction

Both phpdbg and pcov ignore implicit `ZEND_RETURN` paths inserted into functions at compile time and optimized away by opcache.

Credits
=======

The logic for detecting executable opcodes and skipping over non-executable opcodes was lifted from phpdbg

Thanks [@bwoebi](https://github.com/bwoebi) :)
