/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 2019 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
*/
/* $Id$ */

#ifndef PHP_PCOV_H
#define PHP_PCOV_H

extern zend_module_entry pcov_module_entry;
#define phpext_pcov_ptr &pcov_module_entry

#define PHP_PCOV_VERSION "0.0.1"

#ifdef PHP_WIN32
#	define PHP_PCOV_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_PCOV_API __attribute__ ((visibility("default")))
#else
#	define PHP_PCOV_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

typedef struct _php_coverage_t php_coverage_t;

struct _php_coverage_t {
	zend_string    *file;
	uint32_t        line;
	php_coverage_t *next;
};

ZEND_BEGIN_MODULE_GLOBALS(pcov)
	zend_arena     *mem;
	php_coverage_t *start;
	php_coverage_t **next;
	HashTable       files;
	HashTable       ignore;
	HashTable       wants;
	zend_bool       enabled;
	zend_string    *directory;
	struct {
		char   *directory;
		zend_bool enabled;
		zend_long memory;
		zend_long files;
	} ini;
ZEND_END_MODULE_GLOBALS(pcov)

#define PCG(v) ZEND_MODULE_GLOBALS_ACCESSOR(pcov, v)

#if defined(ZTS) && defined(COMPILE_DL_PCOV)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif	/* PHP_PCOV_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
