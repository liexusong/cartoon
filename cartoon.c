/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:  xusong.lie <liexusong@qq.com>                               |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "zend.h"
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_var.h"
#include "php_main.h"
#include "php_smart_str.h"
#include "php_cartoon.h"

#include <signal.h>

/* If you declare any globals in php_cartoon.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(cartoon)
*/

/* True global resources - no need for thread safety here */
static int le_cartoon;

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("cartoon.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_cartoon_globals, cartoon_globals)
    STD_PHP_INI_ENTRY("cartoon.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_cartoon_globals, cartoon_globals)
PHP_INI_END()
*/
/* }}} */

static zval *
backtrace_fetch_args(void **curpos TSRMLS_DC)
{
    void **p = curpos;
    zval *arg_array, **arg;
    int arg_count = (int)(zend_uintptr_t) *p;

    MAKE_STD_ZVAL(arg_array);
    array_init_size(arg_array, arg_count);
    p -= arg_count;

    while (--arg_count >= 0) {
        arg = (zval **) p++;
        if (*arg) {
            if (Z_TYPE_PP(arg) != IS_OBJECT) {
                SEPARATE_ZVAL_TO_MAKE_IS_REF(arg);
            }
            Z_ADDREF_PP(arg);
            add_next_index_zval(arg_array, *arg);
        } else {
            add_next_index_null(arg_array);
        }
    }

    return arg_array;
}

void get_backtrace(zval *retval TSRMLS_DC)
{
    zend_execute_data *ptr, *skip;
    int lineno, frameno = 0;
    const char *function_name;
    const char *filename;
    const char *class_name;
    const char *include_filename = NULL;
    zval *stack_frame;
    /* default params */
    int options = DEBUG_BACKTRACE_PROVIDE_OBJECT, limit = 0;

    ptr = EG(current_execute_data);

    array_init(retval);

    while (ptr && (limit == 0 || frameno < limit)) {
        frameno++;
        MAKE_STD_ZVAL(stack_frame);
        array_init(stack_frame);

        skip = ptr;
        /* skip internal handler */
        if (!skip->op_array
            && skip->prev_execute_data
            && skip->prev_execute_data->opline
            && skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL
            && skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL_BY_NAME
            && skip->prev_execute_data->opline->opcode != ZEND_INCLUDE_OR_EVAL)
        {
            skip = skip->prev_execute_data;
        }

        if (skip->op_array) {
            filename = skip->op_array->filename;
            lineno = skip->opline->lineno;
            add_assoc_string_ex(stack_frame, "file", sizeof("file"), (char*)filename, 1);
            add_assoc_long_ex(stack_frame, "line", sizeof("line"), lineno);

            /* try to fetch args only if an FCALL was just made - elsewise we're in the middle of a function
             * and debug_baktrace() might have been called by the error_handler. in this case we don't
             * want to pop anything of the argument-stack */
        } else {
            zend_execute_data *prev = skip->prev_execute_data;

            while (prev) {
                if (prev->function_state.function &&
                    prev->function_state.function->common.type != ZEND_USER_FUNCTION &&
                    !(prev->function_state.function->common.type == ZEND_INTERNAL_FUNCTION &&
                        (prev->function_state.function->common.fn_flags & ZEND_ACC_CALL_VIA_HANDLER))) {
                    break;
                }

                if (prev->op_array) {
                    add_assoc_string_ex(stack_frame, "file", sizeof("file"), (char*)prev->op_array->filename, 1);
                    add_assoc_long_ex(stack_frame, "line", sizeof("line"), prev->opline->lineno);
                    break;
                }
                prev = prev->prev_execute_data;
            }
            filename = NULL;
        }

        function_name = (ptr->function_state.function->common.scope &&
            ptr->function_state.function->common.scope->trait_aliases) ?
                zend_resolve_method_name(
                    ptr->object ?
                        Z_OBJCE_P(ptr->object) :
                        ptr->function_state.function->common.scope,
                    ptr->function_state.function) :
                ptr->function_state.function->common.function_name;

        if (function_name) {
            add_assoc_string_ex(stack_frame, "function", sizeof("function"), (char*)function_name, 1);

            if (ptr->object && Z_TYPE_P(ptr->object) == IS_OBJECT) {
                if (ptr->function_state.function->common.scope) {
                    add_assoc_string_ex(stack_frame, "class", sizeof("class"), (char*)ptr->function_state.function->common.scope->name, 1);
                } else {
                    zend_uint class_name_len;
                    int dup;

                    dup = zend_get_object_classname(ptr->object, &class_name, &class_name_len TSRMLS_CC);
                    add_assoc_string_ex(stack_frame, "class", sizeof("class"), (char*)class_name, dup);

                }
                if ((options & DEBUG_BACKTRACE_PROVIDE_OBJECT) != 0) {
                    add_assoc_zval_ex(stack_frame, "object", sizeof("object"), ptr->object);
                    Z_ADDREF_P(ptr->object);
                }

                add_assoc_string_ex(stack_frame, "type", sizeof("type"), "->", 1);
            } else if (ptr->function_state.function->common.scope) {
                add_assoc_string_ex(stack_frame, "class", sizeof("class"), (char*)ptr->function_state.function->common.scope->name, 1);
                add_assoc_string_ex(stack_frame, "type", sizeof("type"), "::", 1);
            }

            if ((options & DEBUG_BACKTRACE_IGNORE_ARGS) == 0 &&
                ((! ptr->opline) || ((ptr->opline->opcode == ZEND_DO_FCALL_BY_NAME) || (ptr->opline->opcode == ZEND_DO_FCALL)))) {
                if (ptr->function_state.arguments) {
                    add_assoc_zval_ex(stack_frame, "args", sizeof("args"), backtrace_fetch_args(ptr->function_state.arguments TSRMLS_CC));
                }
            }
        } else {
            /* i know this is kinda ugly, but i'm trying to avoid extra cycles in the main execution loop */
            zend_bool build_filename_arg = 1;

            if (!ptr->opline || ptr->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
                /* can happen when calling eval from a custom sapi */
                function_name = "unknown";
                build_filename_arg = 0;
            } else
            switch (ptr->opline->extended_value) {
                case ZEND_EVAL:
                    function_name = "eval";
                    build_filename_arg = 0;
                    break;
                case ZEND_INCLUDE:
                    function_name = "include";
                    break;
                case ZEND_REQUIRE:
                    function_name = "require";
                    break;
                case ZEND_INCLUDE_ONCE:
                    function_name = "include_once";
                    break;
                case ZEND_REQUIRE_ONCE:
                    function_name = "require_once";
                    break;
                default:
                    /* this can actually happen if you use debug_backtrace() in your error_handler and
                     * you're in the top-scope */
                    function_name = "unknown";
                    build_filename_arg = 0;
                    break;
            }

            if (build_filename_arg && include_filename) {
                zval *arg_array;

                MAKE_STD_ZVAL(arg_array);
                array_init(arg_array);

                /* include_filename always points to the last filename of the last last called-function.
                   if we have called include in the frame above - this is the file we have included.
                 */

                add_next_index_string(arg_array, (char*)include_filename, 1);
                add_assoc_zval_ex(stack_frame, "args", sizeof("args"), arg_array);
            }

            add_assoc_string_ex(stack_frame, "function", sizeof("function"), (char*)function_name, 1);
        }

        add_next_index_zval(retval, stack_frame);

        include_filename = filename;

        ptr = skip->prev_execute_data;
    }
}


void save_backtrace(zval *retval TSRMLS_DC)
{
    smart_str buf = {0};

    php_var_export_ex(&retval, 1, &buf TSRMLS_CC);
    smart_str_0 (&buf);
    php_log_err(buf.c TSRMLS_CC); /* save into log file */
    smart_str_free(&buf);
}


/* {{{ php_cartoon_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_cartoon_init_globals(zend_cartoon_globals *cartoon_globals)
{
	cartoon_globals->global_value = 0;
	cartoon_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */

void sig_handler(int signo)
{
    TSRMLS_FETCH();
    zval retval;

    switch (signo) {
    case SIGSEGV:
        break;
    default:
        return;
    }

    get_backtrace(&retval TSRMLS_CC); /* get backtrace */
    save_backtrace(&retval TSRMLS_CC);
    exit(signo);
}

PHP_MINIT_FUNCTION(cartoon)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/

    signal(SIGSEGV, sig_handler); /* segment fault signal process handler */

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(cartoon)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(cartoon)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(cartoon)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(cartoon)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "cartoon support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


PHP_FUNCTION(cartoon_coredump_debug)
{
    char *ptr = (char *)0;
    *ptr = 1;
}

/* {{{ cartoon_functions[]
 *
 * Every user visible function must have an entry in cartoon_functions[].
 */
const zend_function_entry cartoon_functions[] = {
	PHP_FE(cartoon_coredump_debug,	NULL)
	PHP_FE_END	/* Must be the last line in cartoon_functions[] */
};
/* }}} */

/* {{{ cartoon_module_entry
 */
zend_module_entry cartoon_module_entry = {
	STANDARD_MODULE_HEADER,
	"cartoon",
	cartoon_functions,
	PHP_MINIT(cartoon),
	PHP_MSHUTDOWN(cartoon),
	PHP_RINIT(cartoon),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(cartoon),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(cartoon),
	PHP_CARTOON_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_CARTOON
ZEND_GET_MODULE(cartoon)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
