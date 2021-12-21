/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: d2cebacd7516b58ebb9ce549595ee2b8ce3c3d6d */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcov_start, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

#define arginfo_pcov_stop arginfo_pcov_start

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcov_collect, 0, 0, IS_ARRAY, 1)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, type, IS_LONG, 0, "pcov\\all")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, filter, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcov_clear, 0, 0, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, files, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcov_waiting, 0, 0, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_pcov_memory, 0, 0, IS_LONG, 1)
ZEND_END_ARG_INFO()


ZEND_FUNCTION(start);
ZEND_FUNCTION(stop);
ZEND_FUNCTION(collect);
ZEND_FUNCTION(clear);
ZEND_FUNCTION(waiting);
ZEND_FUNCTION(memory);


static const zend_function_entry ext_functions[] = {
	ZEND_NS_FE("pcov", start, arginfo_pcov_start)
	ZEND_NS_FE("pcov", stop, arginfo_pcov_stop)
	ZEND_NS_FE("pcov", collect, arginfo_pcov_collect)
	ZEND_NS_FE("pcov", clear, arginfo_pcov_clear)
	ZEND_NS_FE("pcov", waiting, arginfo_pcov_waiting)
	ZEND_NS_FE("pcov", memory, arginfo_pcov_memory)
	ZEND_FE_END
};
