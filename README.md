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

Notes
=====

__PCOV is not interoperable with xdebug or phpdbg__

The system ini option ```pcov.directory``` shall restrict the collection of coverage to a single directory.

Without this option set, pcov will record coverage for all files, this is a waste of resources but not a bad default.

__set pcov.directory!!__

The system ini option ```pcov.enabled``` shall enable or disable the installation of the zend hooks that pcov requires to operate; The hard coded default value is ```1```, the recommended default configuration file value should be ```0``` for production and ```1``` for development.

Credits
=======

The logic for detecting executable opcodes and skipping over non-executable opcodes was lifted from phpdbg

Thanks [@bwoebi](https://github.com/bwoebi) :)
