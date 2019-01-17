PCOV
====

A self contained [CodeCoverage](https://github.com/sebastianbergmann/php-code-coverage) compatible driver for PHP7

Requirements
============

  * PHP 7
  * A care free attitude to life

API
===

```php
/*
* Shall start collecting coverage information from Zend
**/
function \pcov\start(void);

/*
* Shall stop collecting coverage information from Zend
**/
function \pcov\stop(void);

/*
* Shall collect coverage information from Zend
* @param integer \pcov\all        shall collect coverage information for all files
*		 \pcov\inclusive  shall collect coverage information for the specified files
*		 \pcov\exclusive  shall collect coverage information for all but the specified files
* @param array   filenames
*/
function \pcov\collect(int $type = \pcov\all, array $filter = []);

/*
* Shall clear stored coverage information
* @param set true to clear code cache
* Note: clearing the code cache may have surprising consequences
*/
function \pcov\clear(bool $code = false);
```

Configuration
=============

  * pcov.enabled          (default: 1)      shall enable or disable zend hooks for pcov
  * pcov.directory        (default: /)      shall restrict collection to a single directory
  * pcov.initial.memory   (default: 65536)  shall set the initial size of the arena used by pcov
  * pcov.initial.files    (default: 64)     shall set the initial size of the files table

The recommended defaults for production should be:

  * pcov.enabled = 0

The recommended defaults for development should be:

  * pcov.enabled = 1
  * pcov.directory = /path/to/your/source/tree

__Note: PCOV is not interoperable with xdebug or phpdbg__

Credits
=======

The logic for detecting executable opcodes and skipping over non-executable opcodes was lifted from phpdbg

Thanks [@bwoebi](https://github.com/bwoebi) :)
