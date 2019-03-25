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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/pcre/php_pcre.h"

#include "zend_arena.h"
#include "zend_cfg.h"
#include "zend_exceptions.h"
#include "zend_vm.h"
#include "zend_vm_opcodes.h"

#include "php_pcov.h"

#define PCOV_FILTER_ALL     0
#define PCOV_FILTER_INCLUDE 1
#define PCOV_FILTER_EXCLUDE 2

#define PHP_PCOV_UNCOVERED   -1
#define PHP_PCOV_COVERED      1

#ifndef GC_ADDREF
#	define GC_ADDREF(g) ++GC_REFCOUNT(g)
#endif

#if PHP_VERSION_ID < 70300
#define php_pcre_pce_incref(c) (c)->refcount++
#define php_pcre_pce_decref(c) (c)->refcount--
#endif

#define PHP_PCOV_API_ENABLED_GUARD() do { \
	if (!INI_BOOL("pcov.enabled")) { \
		return; \
	} \
} while (0);

static zval php_pcov_uncovered;
static zval php_pcov_covered;

void (*zend_execute_ex_function)(zend_execute_data *execute_data);
zend_op_array* (*zend_compile_file_function)(zend_file_handle *fh, int type) = NULL;

ZEND_DECLARE_MODULE_GLOBALS(pcov)

PHP_INI_BEGIN()
	STD_PHP_INI_BOOLEAN(
		"pcov.enabled", "1", 
		PHP_INI_SYSTEM, OnUpdateBool, 
		ini.enabled, zend_pcov_globals, pcov_globals)
	STD_PHP_INI_ENTRY  (
		"pcov.directory", "", 
		PHP_INI_SYSTEM | PHP_INI_PERDIR, OnUpdateString, 
		ini.directory, zend_pcov_globals, pcov_globals)
	STD_PHP_INI_ENTRY  (
		"pcov.exclude", "", 
		PHP_INI_SYSTEM | PHP_INI_PERDIR, OnUpdateString, 
		ini.exclude, zend_pcov_globals, pcov_globals)
	STD_PHP_INI_ENTRY(
		"pcov.initial.memory", "65336", 
		PHP_INI_SYSTEM | PHP_INI_PERDIR, OnUpdateLong, 
		ini.memory, zend_pcov_globals, pcov_globals)
	STD_PHP_INI_ENTRY(
		"pcov.initial.files", "64", 
		PHP_INI_SYSTEM | PHP_INI_PERDIR, OnUpdateLong, 
		ini.files, zend_pcov_globals, pcov_globals)
PHP_INI_END()

static PHP_GINIT_FUNCTION(pcov)
{
#if defined(COMPILE_DL_PCOV) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	ZEND_SECURE_ZERO(pcov_globals, sizeof(zend_pcov_globals));
}

static zend_always_inline zend_bool php_pcov_wants(zend_string *filename) { /* {{{ */
	if (!PCG(directory)) {
		return 1;
	}

	if (ZSTR_LEN(filename) < ZSTR_LEN(PCG(directory))) {
		return 0;
	}

	if (zend_hash_exists(&PCG(wants), filename)) {
		return 1;
	}

	if (zend_hash_exists(&PCG(ignores), filename)) {
		return 0;
	}

	if (strncmp(
		ZSTR_VAL(filename), 
		ZSTR_VAL(PCG(directory)), 
		ZSTR_LEN(PCG(directory))) == SUCCESS) {

		if (PCG(exclude)) {
			zval match;

			ZVAL_UNDEF(&match);

			php_pcre_match_impl(
				PCG(exclude),
#if PHP_VERSION_ID >= 70400
				filename,
#else
				ZSTR_VAL(filename), ZSTR_LEN(filename),
#endif
				&match, NULL,
				0, 0, 0, 0);

			if (zend_is_true(&match)) {
				zend_hash_add_empty_element(
					&PCG(ignores), filename);
				return 0;
			}
		}

		zend_hash_add_empty_element(&PCG(wants), filename);
		return 1;
	}

	zend_hash_add_empty_element(&PCG(ignores), filename);
	return 0;
} /* }}} */

static zend_always_inline zend_bool php_pcov_ignored_opcode(const zend_op *opline, zend_uchar opcode) { /* {{{ */
	return
	    opcode == ZEND_NOP || 
	    opcode == ZEND_OP_DATA || 
 	    opcode == ZEND_FE_FREE || 
	    opcode == ZEND_FREE || 
	    opcode == ZEND_ASSERT_CHECK ||
	    opcode == ZEND_VERIFY_RETURN_TYPE ||  
	    opcode == ZEND_RECV ||
	    opcode == ZEND_RECV_INIT ||
	    opcode == ZEND_RECV_VARIADIC ||
	    opcode == ZEND_SEND_VAL ||
	    opcode == ZEND_SEND_VAR_EX ||
	    opcode == ZEND_SEND_REF ||
	    opcode == ZEND_SEND_UNPACK ||
	    opcode == ZEND_DECLARE_CONST || 
	    opcode == ZEND_DECLARE_CLASS || 
	    opcode == ZEND_DECLARE_INHERITED_CLASS || 
	    opcode == ZEND_DECLARE_FUNCTION || 
	    opcode == ZEND_DECLARE_INHERITED_CLASS_DELAYED || 
	    opcode == ZEND_DECLARE_ANON_CLASS || 
	    opcode == ZEND_DECLARE_ANON_INHERITED_CLASS || 
	    opcode == ZEND_FAST_RET || 
	    opcode == ZEND_FAST_CALL ||
	    opcode == ZEND_TICKS || 
	    opcode == ZEND_EXT_STMT || 
	    opcode == ZEND_EXT_FCALL_BEGIN || 
	    opcode == ZEND_EXT_FCALL_END || 
	    opcode == ZEND_EXT_NOP || 
#if PHP_VERSION_ID < 70400
	    opcode == ZEND_VERIFY_ABSTRACT_CLASS || 
	    opcode == ZEND_ADD_TRAIT || 
	    opcode == ZEND_BIND_TRAITS || 
#endif
	    opcode == ZEND_BIND_GLOBAL
	;
} /* }}} */

static zend_always_inline php_coverage_t* php_pcov_create(zend_execute_data *execute_data) { /* {{{ */
	php_coverage_t *create = (php_coverage_t*) zend_arena_alloc(&PCG(mem), sizeof(php_coverage_t));

	zend_hash_add_empty_element(&PCG(waiting), EX(func)->op_array.filename);

	create->file     = zend_string_copy(EX(func)->op_array.filename);
	create->line     = EX(opline)->lineno;
	create->next     = NULL;

	return create;
} /* }}} */

static zend_always_inline int php_pcov_trace(zend_execute_data *execute_data) { /* {{{ */
	if (PCG(enabled) && php_pcov_wants(EX(func)->op_array.filename)) {
		php_coverage_t *coverage = php_pcov_create(execute_data);

		if (!PCG(start)) {
			PCG(start) = coverage;
		} else {
			*(PCG(next)) = coverage;
		}

		PCG(next) = &coverage->next;
	}

	return zend_vm_call_opcode_handler(execute_data);
} /* }}} */

zend_op_array* php_pcov_compile_file(zend_file_handle *fh, int type) { /* {{{ */
	zend_op_array *result = zend_compile_file_function(fh, type);

	if (!result || !result->filename || !php_pcov_wants(result->filename)) {
		return result;
	}

	if (zend_hash_exists(&PCG(files), result->filename)) {
		return result;
	}

	zend_hash_add_mem(
			&PCG(files), 
			result->filename, 
			result, sizeof(zend_op_array));

#if PHP_VERSION_ID >= 70400
	if (result->refcount) {
		(*result->refcount)++;
	}
	if (result->static_variables) {
		if (!(GC_FLAGS(result->static_variables) & IS_ARRAY_IMMUTABLE)) {
			GC_ADDREF(result->static_variables);
		}
	}
#else
	function_add_ref((zend_function*)result);
#endif

	return result;
} /* }}} */

void php_pcov_execute_ex(zend_execute_data *execute_data) { /* {{{ */
	int zrc        = 0;

	while (1) {
		zrc = php_pcov_trace(execute_data);

		if (zrc != SUCCESS) {
			if (zrc < SUCCESS) {
				return;
			}
			execute_data = EG(current_execute_data);
		}
	}
} /* }}} */

void php_pcov_files_dtor(zval *zv) { /* {{{ */
	destroy_op_array(Z_PTR_P(zv));
	efree(Z_PTR_P(zv));
} /* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(pcov)
{
	REGISTER_NS_LONG_CONSTANT("pcov", "all",         PCOV_FILTER_ALL,     CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("pcov", "inclusive",   PCOV_FILTER_INCLUDE, CONST_CS|CONST_PERSISTENT);
	REGISTER_NS_LONG_CONSTANT("pcov", "exclusive",   PCOV_FILTER_EXCLUDE, CONST_CS|CONST_PERSISTENT);

	REGISTER_NS_STRING_CONSTANT("pcov", "version",     PHP_PCOV_VERSION,    CONST_CS|CONST_PERSISTENT);

	REGISTER_INI_ENTRIES();

	if (INI_BOOL("pcov.enabled")) {
		zend_execute_ex_function   = zend_execute_ex;
		zend_execute_ex            = php_pcov_execute_ex;
	}

	ZVAL_LONG(&php_pcov_uncovered,   PHP_PCOV_UNCOVERED);
	ZVAL_LONG(&php_pcov_covered,     PHP_PCOV_COVERED);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(pcov)
{
	if (INI_BOOL("pcov.enabled")) {
		zend_execute_ex   = zend_execute_ex_function;
	}

	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}
/* }}} */

const char *php_pcov_directory_defaults[] = { /* {{{ */
	"src",
	"lib",
	"app",
	".",
	NULL
}; /* }}} */

static  void php_pcov_setup_directory(char *directory) { /* {{{ */
	char        realpath[MAXPATHLEN];
	zend_stat_t statbuf;

	if (!directory || !*directory) {
		const char** try = php_pcov_directory_defaults;

		while (*try) {
			if (VCWD_REALPATH(*try, realpath) &&
			    VCWD_STAT(realpath, &statbuf) == SUCCESS) {
				directory = realpath;
				break;
			}
			try++;
		}
	} else {
		if (VCWD_REALPATH(directory, realpath) && 
		    VCWD_STAT(realpath, &statbuf) == SUCCESS) {
			directory = realpath;
		}
	}

	PCG(directory) = zend_string_init(directory, strlen(directory), 0);
} /* }}} */

static zend_always_inline void php_pcov_setup_exclude(char *exclude) { /* {{{ */
	zend_string *pattern;

	if (!exclude || !*exclude) {
		return;
	}

	pattern = zend_string_init(
		exclude, strlen(exclude), 0);

	PCG(exclude) = pcre_get_compiled_regex_cache(pattern);

	if (PCG(exclude)) {
		php_pcre_pce_incref(PCG(exclude));
	}

	zend_string_release(pattern);
} /* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(pcov)
{
#if defined(COMPILE_DL_PCOV) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	if (!INI_BOOL("pcov.enabled")) {
		return SUCCESS;
	}

	PCG(mem) = zend_arena_create(INI_INT("pcov.initial.memory"));

	zend_hash_init(&PCG(files),      INI_INT("pcov.initial.files"), NULL, php_pcov_files_dtor, 0);
	zend_hash_init(&PCG(waiting),    INI_INT("pcov.initial.files"), NULL, NULL, 0);
	zend_hash_init(&PCG(ignores),    INI_INT("pcov.initial.files"), NULL, NULL, 0);
	zend_hash_init(&PCG(wants),      INI_INT("pcov.initial.files"), NULL, NULL, 0);
	zend_hash_init(&PCG(discovered), INI_INT("pcov.initial.files"), NULL, ZVAL_PTR_DTOR, 0);

	php_pcov_setup_directory(INI_STR("pcov.directory"));
	php_pcov_setup_exclude(INI_STR("pcov.exclude"));

#ifdef ZEND_COMPILE_NO_JUMPTABLES
	CG(compiler_options) |= ZEND_COMPILE_NO_JUMPTABLES;
#endif

	if  (!zend_compile_file_function) {
		zend_compile_file_function = zend_compile_file;
		zend_compile_file          = php_pcov_compile_file;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(pcov)
{
	if (!INI_BOOL("pcov.enabled") || CG(unclean_shutdown)) {
		return SUCCESS;
	}

	if (PCG(start)) {
		php_coverage_t *coverage = PCG(start);
		do {
			zend_string_release(coverage->file);
		} while ((coverage = coverage->next));
	}

	zend_hash_destroy(&PCG(files));
	zend_hash_destroy(&PCG(ignores));
	zend_hash_destroy(&PCG(wants));
	zend_hash_destroy(&PCG(discovered));
	zend_hash_destroy(&PCG(waiting));

	zend_arena_destroy(PCG(mem));

	if (PCG(directory)) {
		zend_string_release(PCG(directory));
	}

	if (PCG(exclude)) {
		php_pcre_pce_decref(PCG(exclude));
	}

	if (zend_compile_file == php_pcov_compile_file) {
		zend_compile_file = zend_compile_file_function;
		zend_compile_file_function = NULL;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(pcov)
{
	char info[64];
	char *directory = INI_STR("pcov.directory");
	char *exclude   = INI_STR("pcov.exclude");

	php_info_print_table_start();

	php_info_print_table_header(2,
		"PCOV support",
		INI_BOOL("pcov.enabled")  ? "Enabled" : "Disabled");
	php_info_print_table_row(2,
		"PCOV version",
		PHP_PCOV_VERSION);
	php_info_print_table_row(2,
		"pcov.directory",
		directory && *directory ? directory : "auto");
	php_info_print_table_row(2,
		"pcov.exclude",
		exclude   && *exclude   ? exclude : "none" );

	snprintf(info, sizeof(info),
		ZEND_LONG_FMT " bytes",
		(zend_long) INI_INT("pcov.initial.memory"));
	php_info_print_table_row(2,
		"pcov.initial.memory", info);

	snprintf(info, sizeof(info),
		ZEND_LONG_FMT,
		(zend_long) INI_INT("pcov.initial.files"));
	php_info_print_table_row(2,
		"pcov.initial.files", info);

	php_info_print_table_end();
}
/* }}} */

static zend_always_inline void php_pcov_report(php_coverage_t *coverage, zval *filter) { /* {{{ */
	zval *table;
	zval *hit;

	if (!coverage) {
		return;
	}

	do {
		if ((table = zend_hash_find(Z_ARRVAL_P(filter), coverage->file))) {
			if ((hit = zend_hash_index_find(Z_ARRVAL_P(table), coverage->line))) {
				Z_LVAL_P(hit) = PHP_PCOV_COVERED;
			}
		}
	} while ((coverage = coverage->next));
} /* }}} */

static zend_always_inline void php_pcov_discover_code(zend_op_array *ops, zval *return_value) { /* {{{ */
	zend_cfg cfg;
	zend_basic_block *block;
	zend_op *limit = ops->opcodes + ops->last;
	int i = 0;
	void *check = NULL;
    
	if (ops->fn_flags & ZEND_ACC_ABSTRACT) {
		return;
	}

	memset(&cfg, 0, sizeof(zend_cfg));

	check = zend_arena_checkpoint(PCG(mem));

	zend_build_cfg(&PCG(mem), ops,  ZEND_RT_CONSTANTS, &cfg);

	for (block = cfg.blocks, i = 0; i < cfg.blocks_count; i++, block++) {
		zend_op *opline = ops->opcodes + block->start, 
			*end = opline + block->len;

		if (!(block->flags & ZEND_BB_REACHABLE)) {
			/*
			* Note that, we don't care about unreachable blocks
			* that would be removed by opcache, because it would
			* create different reports depending on configuration
			*/
			continue;
		}

		while(opline < end) {
			if (php_pcov_ignored_opcode(opline, opline->opcode)) {
				opline++;
				continue;
			}

			if (!zend_hash_index_exists(Z_ARRVAL_P(return_value), opline->lineno)) {
				zend_hash_index_add(
					Z_ARRVAL_P(return_value), 
					opline->lineno, &php_pcov_uncovered);
			}

			if ((opline +0)->opcode == ZEND_NEW && 
			    (opline +1)->opcode == ZEND_DO_FCALL) {
				opline++;
			}

			opline++;
		}

		if (block == cfg.blocks && opline == limit) {
			/*
			* If the first basic block finishes at the end of the op array
			* then we don't care about subsequent blocks
			*/
			break;
		}
	}
	
	zend_arena_release(&PCG(mem), check);
} /* }}} */

static zend_always_inline void php_pcov_discover_file(zend_string *file, zval *return_value) { /* {{{ */
	zval discovered;
	zend_op_array *ops;
	zval *cache = zend_hash_find(&PCG(discovered), file);

	if (cache) {
		zend_hash_update(
			Z_ARRVAL_P(return_value), file, cache);
		Z_ADDREF_P(cache);
		return;
	}

	if (!(ops = zend_hash_find_ptr(&PCG(files), file))) {
		return;
	}

	array_init(&discovered);

	php_pcov_discover_code(ops, &discovered);
	{
		zend_class_entry *ce;
		zend_op_array    *function;
		ZEND_HASH_FOREACH_PTR(EG(class_table), ce) {
			if (ce->type != ZEND_USER_CLASS) {
				continue;
			}

			ZEND_HASH_FOREACH_PTR(&ce->function_table, function) {
				if (function->type == ZEND_USER_FUNCTION &&
				    function->filename &&
				    zend_string_equals(file, function->filename)) {
					php_pcov_discover_code(function, &discovered);
				}
			} ZEND_HASH_FOREACH_END();
		} ZEND_HASH_FOREACH_END();
	}

	{
		zend_op_array *function;
		ZEND_HASH_FOREACH_PTR(EG(function_table), function) {
			if (function->type == ZEND_USER_FUNCTION &&
			    function->filename &&
			    zend_string_equals(file, function->filename)) {
				php_pcov_discover_code(function, &discovered);
			}
		} ZEND_HASH_FOREACH_END();
	}

	zend_hash_update(Z_ARRVAL_P(return_value), file, &discovered);
	zend_hash_update(&PCG(discovered), file, &discovered);
	Z_ADDREF(discovered);
} /* }}} */

/* {{{ array \pcov\collect(int $type = \pcov\all, array $filter = []); */
PHP_NAMED_FUNCTION(php_pcov_collect)
{
	zend_long type = PCOV_FILTER_ALL;
	zval      *filter = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|la", &type, &filter) != SUCCESS) {
		return;
	}

	PHP_PCOV_API_ENABLED_GUARD();

	if (PCOV_FILTER_ALL != type &&
	    PCOV_FILTER_INCLUDE != type &&
	    PCOV_FILTER_EXCLUDE != type) {
		zend_throw_error(zend_ce_type_error, 
			"type must be "
				"\\pcov\\inclusive, "
				"\\pcov\\exclusive, or \\pcov\\all");
		return;
	}

	array_init(return_value);

	if (PCG(last) == PCG(next)) {
		return;
	}

	PCG(last) = PCG(next);

	switch(type) {
		case PCOV_FILTER_INCLUDE: {
			zval *filtered;
			ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(filter), filtered) {
				if (Z_TYPE_P(filtered) != IS_STRING) {
					continue;
				}

				php_pcov_discover_file(Z_STR_P(filtered), return_value);
			} ZEND_HASH_FOREACH_END();
		} break;

		case PCOV_FILTER_EXCLUDE: {
			zend_string *name;
			zval *filtered;
			ZEND_HASH_FOREACH_STR_KEY(&PCG(files), name) {
				ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(filter), filtered) {
					if (Z_TYPE_P(filtered) != IS_STRING) {
						continue;
					}

					if (zend_string_equals(name, Z_STR_P(filtered))) {
						goto _php_pcov_collect_exclude;
					}
				} ZEND_HASH_FOREACH_END();				
				php_pcov_discover_file(name, return_value);

			_php_pcov_collect_exclude:
				continue;
			} ZEND_HASH_FOREACH_END();
		} break;

		case PCOV_FILTER_ALL: {
			zend_string *name;
			ZEND_HASH_FOREACH_STR_KEY(&PCG(files), name) {
				php_pcov_discover_file(name, return_value);
			} ZEND_HASH_FOREACH_END();
		} break;
	}

	php_pcov_report(PCG(start), return_value);
} /* }}} */

/* {{{ void \pcov\start(void) */
PHP_NAMED_FUNCTION(php_pcov_start)
{
	if (zend_parse_parameters_none() != SUCCESS) {
		return;
	}

	PHP_PCOV_API_ENABLED_GUARD();

	PCG(enabled) = 1;
} /* }}} */

/* {{{ void \pcov\stop(void) */
PHP_NAMED_FUNCTION(php_pcov_stop)
{
	if (zend_parse_parameters_none() != SUCCESS) {
		return;
	}

	PHP_PCOV_API_ENABLED_GUARD();

	PCG(enabled) = 0;
} /* }}} */

/* {{{ void \pcov\clear(bool $files = false) */
PHP_NAMED_FUNCTION(php_pcov_clear)
{
	zend_bool files = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &files) != SUCCESS) {
		return;
	}

	PHP_PCOV_API_ENABLED_GUARD();

	if (PCG(start)) {
		php_coverage_t *coverage = PCG(start);
		do {
			zend_string_release(coverage->file);
		} while ((coverage = coverage->next));
	}

	if (files) {
		zend_hash_clean(&PCG(files));
		zend_hash_clean(&PCG(discovered));
	}

	zend_arena_destroy(PCG(mem));
	zend_hash_clean(&PCG(waiting));

	PCG(mem) = zend_arena_create(INI_INT("pcov.initial.memory"));
	PCG(start) = NULL;
} /* }}} */

/* {{{ array \pcov\waiting(void) */
PHP_NAMED_FUNCTION(php_pcov_waiting) 
{
	zend_string *waiting;

	if (zend_parse_parameters_none() != SUCCESS) {
		return;	
	}

	PHP_PCOV_API_ENABLED_GUARD();

	array_init(return_value);

	ZEND_HASH_FOREACH_STR_KEY(&PCG(waiting), waiting) {
		add_next_index_str(
			return_value, 
			zend_string_copy(waiting));
	} ZEND_HASH_FOREACH_END();
} /* }}} */

/* {{{ int \pcov\memory(void) */
PHP_NAMED_FUNCTION(php_pcov_memory) 
{
	zend_arena *arena = PCG(mem);

	if (zend_parse_parameters_none() != SUCCESS) {
		return;
	}

	PHP_PCOV_API_ENABLED_GUARD();

	ZVAL_LONG(return_value, 0);

	do {
		Z_LVAL_P(return_value) += (arena->end - arena->ptr);
	} while ((arena = arena->prev));
} /* }}} */

/* {{{ */
ZEND_BEGIN_ARG_INFO_EX(php_pcov_collect_arginfo, 0, 0, 0)
	ZEND_ARG_TYPE_INFO(0, type, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, filter, IS_ARRAY, 0)
ZEND_END_ARG_INFO() /* }}} */

/* {{{ */
ZEND_BEGIN_ARG_INFO_EX(php_pcov_clear_arginfo, 0, 0, 0)
	ZEND_ARG_TYPE_INFO(0, files, _IS_BOOL, 0)
ZEND_END_ARG_INFO() /* }}} */

/* {{{ */
ZEND_BEGIN_ARG_INFO_EX(php_pcov_no_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO() /* }}} */

/* {{{ php_pcov_functions[]
 */
const zend_function_entry php_pcov_functions[] = {
	ZEND_NS_FENTRY("pcov", start,      php_pcov_start,         php_pcov_no_arginfo, 0)
	ZEND_NS_FENTRY("pcov", stop,       php_pcov_stop,          php_pcov_no_arginfo, 0)
	ZEND_NS_FENTRY("pcov", collect,    php_pcov_collect,       php_pcov_collect_arginfo, 0)
	ZEND_NS_FENTRY("pcov", clear,      php_pcov_clear,         php_pcov_clear_arginfo, 0)
	ZEND_NS_FENTRY("pcov", waiting,    php_pcov_waiting,       php_pcov_no_arginfo, 0)
	ZEND_NS_FENTRY("pcov", memory,     php_pcov_memory,        php_pcov_no_arginfo, 0)
	PHP_FE_END
};
/* }}} */

/* {{{ pcov_module_deps[] */
static const zend_module_dep pcov_module_deps[] = {
	ZEND_MOD_REQUIRED("pcre")
	{NULL, NULL, NULL}
}; /* }}} */

/* {{{ pcov_module_entry
 */
zend_module_entry pcov_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	pcov_module_deps,
	"pcov",
	php_pcov_functions,
	PHP_MINIT(pcov),
	PHP_MSHUTDOWN(pcov),
	PHP_RINIT(pcov),
	PHP_RSHUTDOWN(pcov),
	PHP_MINFO(pcov),
	PHP_PCOV_VERSION,
	PHP_MODULE_GLOBALS(pcov),
	PHP_GINIT(pcov),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_PCOV
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(pcov)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
