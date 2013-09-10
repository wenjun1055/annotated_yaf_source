/*
   +----------------------------------------------------------------------+
   | Yet Another Framework                                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Xinchen Hui  <laruence@php.net>                              |
   +----------------------------------------------------------------------+
*/

/* $Id: yaf_request.c 328820 2012-12-18 08:16:24Z remi $*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_alloc.h"
#include "ext/standard/php_string.h"

#include "php_yaf.h"
#include "yaf_request.h"
#include "yaf_namespace.h"
#include "yaf_exception.h"

#include "requests/simple.c"
#include "requests/http.c"

zend_class_entry *yaf_request_ce;

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(yaf_request_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_set_routed_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, flag)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_set_module_name_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, module)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_set_controller_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, controller)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_set_action_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, action)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_set_baseuir_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, uir)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_set_request_uri_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, uir)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_set_param_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_get_param_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, default)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_getserver_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, default)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_request_getenv_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, default)
ZEND_END_ARG_INFO()
/* }}} */

/** {{{ yaf_request_t * yaf_request_instance(zval *this_ptr, char *other TSRMLS_DC)
 *	产生一个yaf_request_http的实例，并且返回
*/
yaf_request_t * yaf_request_instance(yaf_request_t *this_ptr, char *other TSRMLS_DC) {
	yaf_request_t *instance = yaf_request_http_instance(this_ptr, NULL, other TSRMLS_CC);
	return instance;
}
/* }}} */

/** {{{ int yaf_request_set_base_uri(yaf_request_t *request, char *base_uri, char *request_uri TSRMLS_DC)
*/
int yaf_request_set_base_uri(yaf_request_t *request, char *base_uri, char *request_uri TSRMLS_DC) {
	char *basename = NULL;
	uint basename_len = 0;
	zval *container = NULL;		//没看出这个有啥用，赋值了后啥都没做就直接释放了

	if (!base_uri) {
		/* 没有传入base_uri，就自己分析找到base_uri */
		zval 	*script_filename;
		char 	*file_name, *ext = YAF_G(ext);
		size_t 	file_name_len;
		uint  	ext_len;

		ext_len	= strlen(ext);
		/* $_SEVER['SCRIPT_FILENAME'] 当前执行脚本的绝对路径 */
		script_filename = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("SCRIPT_FILENAME") TSRMLS_CC);

		/**
    	 *	if (isset($_SERVER['SCRIPT_FILENAME'])) {
		 *		if (isset($_SERVER['SCRIPT_NAME']) && basename($_SEVER['SCRIPT_FILENAME'], '.php') == basename($_SEVER['SCRIPT_NAME'])) {
    	 *			$basename = $_SERVER['SCRIPT_NAME']; 		
    	 *		} elseif (isset($_SERVER['PHP_SELF']) && basename($_SEVER['SCRIPT_FILENAME'], '.php') == basename($_SEVER['PHP_SELF'])) {
		 *			$basename = $_SERVER['PHP_SELF'];
    	 *		} elseif (isset($_SERVER['ORIG_SCRIPT_NAME']) && basename($_SEVER['SCRIPT_FILENAME'], '.php') == basename($_SEVER['ORIG_SCRIPT_NAME'])) {
		 *			$basename = $_SERVER['ORIG_SCRIPT_NAME'];
    	 *		}
    	 *	}
		 */

		do {
			if (script_filename && IS_STRING == Z_TYPE_P(script_filename)) {
				zval *script_name, *phpself_name, *orig_name;
				/* $_SEVER['SCRIPT_NAME'] 包含当前脚本的路径 */
				script_name = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("SCRIPT_NAME") TSRMLS_CC);
				/* basename($_SEVER['SCRIPT_FILENAME'], '.php')的实现，获取文件的名称，不带后缀 */
				php_basename(Z_STRVAL_P(script_filename), Z_STRLEN_P(script_filename), ext, ext_len, &file_name, &file_name_len TSRMLS_CC);
				if (script_name && IS_STRING == Z_TYPE_P(script_name)) {
					char 	*script;
					size_t 	script_len;
					/* basename($_SEVER['SCRIPT_NAME'], '.php') */
					php_basename(Z_STRVAL_P(script_name), Z_STRLEN_P(script_name),
							NULL, 0, &script, &script_len TSRMLS_CC);
					if (strncmp(file_name, script, file_name_len) == 0) {
						basename 	 = Z_STRVAL_P(script_name);
						basename_len = Z_STRLEN_P(script_name);
						container = script_name;
						efree(file_name);
						efree(script);
						break;
					}
					efree(script);
				}
				zval_ptr_dtor(&script_name);

				phpself_name = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("PHP_SELF") TSRMLS_CC);
				if (phpself_name && IS_STRING == Z_TYPE_P(phpself_name)) {
					char 	*phpself;
					size_t	phpself_len;

					php_basename(Z_STRVAL_P(phpself_name), Z_STRLEN_P(phpself_name), NULL, 0, &phpself, &phpself_len TSRMLS_CC);
					/* TODO：两个一样说明什么问题呢 */
					if (strncmp(file_name, phpself, file_name_len) == 0) {
						basename	 = Z_STRVAL_P(phpself_name);
						basename_len = Z_STRLEN_P(phpself_name);
						container = phpself_name;
						efree(file_name);
						efree(phpself);
						break;
					}
					efree(phpself);
				}
				zval_ptr_dtor(&phpself_name);
				/** 
				 *	$SERVER['ORIG_SCRIPT_NAME'] 
				 *	要知道PHP当前是通过CGI来运行，还是在Apache内部运行，可以检查一下环境变量orig_script_name。
				 *	如果PHP通过CGI来运行，这个变量的值就是/Php/Php.exe。
				 *	如果Apache将PHP脚本作为模块来运行，该变量的值应该是/Phptest.php
				 */
				orig_name = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("ORIG_SCRIPT_NAME") TSRMLS_CC);
				if (orig_name && IS_STRING == Z_TYPE_P(orig_name)) {
					char 	*orig;
					size_t	orig_len;
					php_basename(Z_STRVAL_P(orig_name), Z_STRLEN_P(orig_name), NULL, 0, &orig, &orig_len TSRMLS_CC);
					if (strncmp(file_name, orig, file_name_len) == 0) {
						basename 	 = Z_STRVAL_P(orig_name);
						basename_len = Z_STRLEN_P(orig_name);
						container = orig_name;
						efree(file_name);
						efree(orig);
						break;
					}
					efree(orig);
				}
				zval_ptr_dtor(&orig_name);
				efree(file_name);
			}
		} while (0);
		zval_ptr_dtor(&script_filename);

		if (basename && strstr(request_uri, basename) == request_uri) {
			/* basename字符串最后一个字符是'/'的话，则去掉最后一个 */
			if (*(basename + basename_len - 1) == '/') {
				--basename_len;
			}
			/* 设置$_base_uri */
			zend_update_property_stringl(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_BASE), basename, basename_len TSRMLS_CC);
			if (container) {
				zval_ptr_dtor(&container);
			}

			return 1;
		} else if (basename) {
			size_t  dir_len;
			char 	*dir = estrndup(basename, basename_len); /* php_dirname might alter the string */

			dir_len = php_dirname(dir, basename_len);
			if (*(basename + dir_len - 1) == '/') {
				--dir_len;
			}

			if (dir_len) {
				if (strstr(request_uri, dir) == request_uri) {
					zend_update_property_string(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_BASE), dir TSRMLS_CC);
					efree(dir);

					if (container) {
						zval_ptr_dtor(&container);
					}
					return 1;
				}
			}
			efree(dir);
		}

		if (container) {
			zval_ptr_dtor(&container);
		}

		zend_update_property_string(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_BASE), "" TSRMLS_CC);
		return 1;
	} else {
		/* 如果传了base_uri,则将它添加到类的成员变量$_base_uri */
		zend_update_property_string(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_BASE), base_uri TSRMLS_CC);
		return 1;
	}
}
/* }}} */

/** {{{ zval * yaf_request_query(uint type, char * name, uint len TSRMLS_DC)
*	通过type来选择从哪个全局变量数组中去查找name的所对应的值
*/
zval * yaf_request_query(uint type, char * name, uint len TSRMLS_DC) {
	zval 		**carrier = NULL, **ret;

/* 
 *	判断接下来用到的_POST、$_GET等全局变量的初始化是在脚本开始运行时候还是在需要用到的时候
 *	http://cn2.php.net/manual/zh/ini.core.php#ini.auto-globals-jit
 */
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4)
	zend_bool 	jit_initialization = (PG(auto_globals_jit) && !PG(register_globals) && !PG(register_long_arrays));
#else
	zend_bool 	jit_initialization = PG(auto_globals_jit);
#endif

	/* for phpunit test requirements */
#if PHP_YAF_DEBUG
	switch (type) {
		case YAF_GLOBAL_VARS_POST:
			/* 全局符号表中查找$_POST */
			(void)zend_hash_find(&EG(symbol_table), ZEND_STRS("_POST"), (void **)&carrier);
			break;
		case YAF_GLOBAL_VARS_GET:
			/* 全局符号表中查找$_GET */
			(void)zend_hash_find(&EG(symbol_table), ZEND_STRS("_GET"), (void **)&carrier);
			break;
		case YAF_GLOBAL_VARS_COOKIE:
			/* 全局符号表中查找$_COOKIE */
			(void)zend_hash_find(&EG(symbol_table), ZEND_STRS("_COOKIE"), (void **)&carrier);
			break;
		case YAF_GLOBAL_VARS_SERVER:
			/* 全局符号表中查找$_SERVER */
			if (jit_initialization) {
				/**
				 *	在通过$获取变量时，PHP内核都会通过这些变量名区分是否为全局变量（ZEND_FETCH_GLOBAL）， 
				 *	其调用的判断函数为zend_is_auto_global，这个过程是在生成中间代码过程中实现的。
				 *	http://www.php-internals.com/book/?p=chapt03/03-03-pre-defined-variable
				 *	TODO Why
				 */
				zend_is_auto_global(ZEND_STRL("_SERVER") TSRMLS_CC);
			}
			(void)zend_hash_find(&EG(symbol_table), ZEND_STRS("_SERVER"), (void **)&carrier);
			break;
		case YAF_GLOBAL_VARS_ENV:
			/* 全局符号表中查找$_ENV */
			if (jit_initialization) {
				zend_is_auto_global(ZEND_STRL("_ENV") TSRMLS_CC);
			}
			carrier = &PG(http_globals)[YAF_GLOBAL_VARS_ENV];
			break;
		case YAF_GLOBAL_VARS_FILES:
			/* 取PHP全局变量数组的成员$_FILES */
			carrier = &PG(http_globals)[YAF_GLOBAL_VARS_FILES];
			break;
		case YAF_GLOBAL_VARS_REQUEST:
			/* 全局符号表中查找$_REQUEST */
			if (jit_initialization) {
				zend_is_auto_global(ZEND_STRL("_REQUEST") TSRMLS_CC);
			}
			(void)zend_hash_find(&EG(symbol_table), ZEND_STRS("_REQUEST"), (void **)&carrier);
			break;
		default:
			break;
	}
#else
	switch (type) {
		case YAF_GLOBAL_VARS_POST:
		case YAF_GLOBAL_VARS_GET:
		case YAF_GLOBAL_VARS_FILES:
		case YAF_GLOBAL_VARS_COOKIE:
			carrier = &PG(http_globals)[type];
			break;
		case YAF_GLOBAL_VARS_ENV:
			if (jit_initialization) {
				zend_is_auto_global(ZEND_STRL("_ENV") TSRMLS_CC);
			}
			carrier = &PG(http_globals)[type];
			break;
		case YAF_GLOBAL_VARS_SERVER:
			if (jit_initialization) {
				zend_is_auto_global(ZEND_STRL("_SERVER") TSRMLS_CC);
			}
			carrier = &PG(http_globals)[type];
			break;
		case YAF_GLOBAL_VARS_REQUEST:
			if (jit_initialization) {
				zend_is_auto_global(ZEND_STRL("_REQUEST") TSRMLS_CC);
			}
			//TODO 为什么和前面的不同呢
			(void)zend_hash_find(&EG(symbol_table), ZEND_STRS("_REQUEST"), (void **)&carrier);
			break;
		default:
			break;
	}
#endif

	if (!carrier || !(*carrier)) {
		/* 获取失败，产生一个NULL，并返回 */
		zval *empty;
		MAKE_STD_ZVAL(empty);
		ZVAL_NULL(empty);
		return empty;
	}

	if (!len) {
		/* 如果没有传入name，则直接将上面查找到的全局变量数组直接返回 */
		Z_ADDREF_P(*carrier);
		return *carrier;
	}
	/* 从上面找到的全局变量数组中去查找name对应的值 */
	if (zend_hash_find(Z_ARRVAL_PP(carrier), name, len + 1, (void **)&ret) == FAILURE) {
		/* 查找失败，返回NULL */
		zval *empty;
		MAKE_STD_ZVAL(empty);
		ZVAL_NULL(empty);
		return empty;
	}
	/* 查找成功，返回找到的值 */
	Z_ADDREF_P(*ret);
	return *ret;
}
/* }}} */

/** {{{ yaf_request_t * yaf_request_get_method(yaf_request_t *request TSRMLS_DC)
*/
yaf_request_t * yaf_request_get_method(yaf_request_t *request TSRMLS_DC) {
	yaf_request_t *method = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_METHOD), 1 TSRMLS_CC);
	return method;
}
/* }}} */

/** {{{ yaf_request_t * yaf_request_get_language(yaf_request_t *instance TSRMLS_DC)
*/
zval * yaf_request_get_language(yaf_request_t *instance TSRMLS_DC) {
	zval *lang;
	//获取$language
	lang = zend_read_property(yaf_request_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_LANG), 1 TSRMLS_CC);

	if (IS_STRING != Z_TYPE_P(lang)) {
		/* $_SERVER['HTTP_ACCEPT_LANGUAGE'] */
		zval * accept_langs = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("HTTP_ACCEPT_LANGUAGE") TSRMLS_CC);

		if (IS_STRING != Z_TYPE_P(accept_langs) || !Z_STRLEN_P(accept_langs)) {
			return accept_langs;
		} else {
			char  	*ptrptr, *seg;
			uint	prefer_len = 0;
			double	max_qvlaue = 0;
			char 	*prefer = NULL;
			char  	*langs = estrndup(Z_STRVAL_P(accept_langs), Z_STRLEN_P(accept_langs));	/* 复制 */

			seg = php_strtok_r(langs, ",", &ptrptr);	/* 截取 strtok() */
			while(seg) {
				char *qvalue;
				while( *(seg) == ' ') seg++ ;
				/* Accept-Language: da, en-gb;q=0.8, en;q=0.7 */
				if ((qvalue = strstr(seg, "q="))) {
					float qval = (float)zend_string_to_double(qvalue + 2, seg - qvalue + 2);
					if (qval > max_qvlaue) {
						max_qvlaue = qval;
						if (prefer) {
							efree(prefer);
						}
						prefer_len = qvalue - seg - 1;
						prefer 	   = estrndup(seg, prefer_len);
					}
				} else {
					if (max_qvlaue < 1) {
						max_qvlaue = 1;
						prefer_len = strlen(seg);
						prefer 	   = estrndup(seg, prefer_len);
					}
				}

				seg = php_strtok_r(NULL, ",", &ptrptr);
			}

			if (prefer) {
				zval *accept_language;
				MAKE_STD_ZVAL(accept_language);
				ZVAL_STRINGL(accept_language,  prefer, prefer_len, 1);
				zend_update_property(yaf_request_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_LANG), accept_language TSRMLS_CC);
				efree(prefer);
				efree(langs);
				return accept_language;
			}
			efree(langs);
		}
	}
	return lang;
}
/* }}} */

/** {{{  int yaf_request_is_routed(yaf_request_t *request TSRMLS_DC)
*/
 int yaf_request_is_routed(yaf_request_t *request TSRMLS_DC) {
	yaf_request_t *routed = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ROUTED), 1 TSRMLS_CC);
	return Z_LVAL_P(routed);
}
/* }}} */

/** {{{  int yaf_request_is_dispatched(yaf_request_t *request TSRMLS_DC)
*/
 int yaf_request_is_dispatched(yaf_request_t *request TSRMLS_DC) {
 	/* 获取$routed并返回值 */
	zval *dispatched = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_STATE), 1 TSRMLS_CC);
	return Z_LVAL_P(dispatched);
}
/* }}} */

/** {{{  int yaf_request_set_dispatched(yaf_request_t *instance, int flag TSRMLS_DC)
*/
 int yaf_request_set_dispatched(yaf_request_t *instance, int flag TSRMLS_DC) {
 	/* 设置$dispatched，并返回1 */
	zend_update_property_bool(yaf_request_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_STATE), flag TSRMLS_CC);
	return 1;
}
/* }}} */

/** {{{  int yaf_request_set_routed(yaf_request_t *request, int flag TSRMLS_DC)
*/
 int yaf_request_set_routed(yaf_request_t *request, int flag TSRMLS_DC) {
 	/* 返回$routed并且返回1 */
	zend_update_property_bool(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ROUTED), flag TSRMLS_CC);
	return 1;
}
/* }}} */

/** {{{  int yaf_request_set_params_single(yaf_request_t *request, char *key, int len, zval *value TSRMLS_DC)
*/
 int yaf_request_set_params_single(yaf_request_t *request, char *key, int len, zval *value TSRMLS_DC) {
 	/* 获取$params */
	zval *params = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), 1 TSRMLS_CC);
	/* 将传递进来的key和value添加到数组params中 */
	if (zend_hash_update(Z_ARRVAL_P(params), key, len+1, &value, sizeof(zval *), NULL) == SUCCESS) {
		Z_ADDREF_P(value);
		return 1;
	}

	return 0;
}
/* }}} */

/** {{{  int yaf_request_set_params_multi(yaf_request_t *request, zval *values TSRMLS_DC)
*/
 int yaf_request_set_params_multi(yaf_request_t *request, zval *values TSRMLS_DC) {
 	/* 获取$params */
	zval *params = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), 1 TSRMLS_CC);
	if (values && Z_TYPE_P(values) == IS_ARRAY) {
		/* 将数组values复制到params */
		zend_hash_copy(Z_ARRVAL_P(params), Z_ARRVAL_P(values), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		return 1;
	}
	return 0;
}
/* }}} */

/** {{{  zval * yaf_request_get_param(yaf_request_t *request, char *key, int len TSRMLS_DC)
*/
 zval * yaf_request_get_param(yaf_request_t *request, char *key, int len TSRMLS_DC) {
	zval **ppzval;
	/* 获取$params */
	zval *params = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), 1 TSRMLS_CC);
	/* hash查找并且返回 */
	if (zend_hash_find(Z_ARRVAL_P(params), key, len + 1, (void **) &ppzval) == SUCCESS) {
		return *ppzval;
	}
	return NULL;
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::isGet(void)
*/
YAF_REQUEST_IS_METHOD(Get);
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::isPost(void)
*/
YAF_REQUEST_IS_METHOD(Post);
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::isPut(void)
*/
YAF_REQUEST_IS_METHOD(Put);
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::isHead(void)
*/
YAF_REQUEST_IS_METHOD(Head);
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::isOptions(void)
*/
YAF_REQUEST_IS_METHOD(Options);
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::isCli(void)
*/
YAF_REQUEST_IS_METHOD(Cli);
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::isXmlHttpRequest(void)
*/
PHP_METHOD(yaf_request, isXmlHttpRequest) {
	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getEnv(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request, Env, 	YAF_GLOBAL_VARS_ENV);
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getServer(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request, Server, YAF_GLOBAL_VARS_SERVER);
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getModuleName(void)
*/
/* 获取$module */
PHP_METHOD(yaf_request, getModuleName) {
	zval *module = zend_read_property(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE), 1 TSRMLS_CC);
	RETVAL_ZVAL(module, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getControllerName(void)
*/
/* 获取$controller */
PHP_METHOD(yaf_request, getControllerName) {
	zval *controller = zend_read_property(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), 1 TSRMLS_CC);
	RETVAL_ZVAL(controller, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getActionName(void)
*/
/* 获取$action */
PHP_METHOD(yaf_request, getActionName) {
	zval *action = zend_read_property(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), 1 TSRMLS_CC);
	RETVAL_ZVAL(action, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::setModuleName(string $module)
*/
PHP_METHOD(yaf_request, setModuleName) {
	zval *module;
	yaf_request_t *self	= getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &module) == FAILURE) {
		return;
	}
	/* module名字必须是一个字符串 */
	if (Z_TYPE_P(module) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expect a string module name");
		RETURN_FALSE;
	}
	/* $this->module = $module */
	zend_update_property(yaf_request_ce, self, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE), module TSRMLS_CC);
	/* return $this; */
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::setControllerName(string $controller)
*/
PHP_METHOD(yaf_request, setControllerName) {
	zval *controller;
	yaf_request_t *self	= getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &controller) == FAILURE) {
		return;
	}
	/* controller名字必须是一个字符串 */
	if (Z_TYPE_P(controller) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expect a string controller name");
		RETURN_FALSE;
	}
	/* $this->controller = $controller */
	zend_update_property(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), controller TSRMLS_CC);
	/* return $this; */
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::setActionName(string $action)
*/
PHP_METHOD(yaf_request, setActionName) {
	zval *action;
	zval *self	 = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &action) == FAILURE) {
		return;
	}
	/* action名字必须是一个字符串 */
	if (Z_TYPE_P(action) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expect a string action name");
		RETURN_FALSE;
	}
	/* $this->action = $action */
	zend_update_property(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), action TSRMLS_CC);
	/* return $this; */
	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::setParam(mixed $value)
*/
PHP_METHOD(yaf_request, setParam) {
	uint argc;
	yaf_request_t *self	= getThis();
	/* 获取传递进来的参数的个数 */
	argc = ZEND_NUM_ARGS();

	if (1 == argc) {
		/* 如果传递进来的参数只有一个，则认为这个参数为一个数组，利用数组的方式来进行复制 */
		zval *value ;
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &value) == FAILURE) {
			return;
		}
		if (yaf_request_set_params_multi(self, value TSRMLS_CC)) {
			RETURN_ZVAL(self, 1, 0);
		}
	} else if (2 == argc) {
		/* 如果传递进来的参数为两个，则将它们作为键值对来进行保存 */
		zval *value;
		char *name;
		uint len;
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &len, &value) == FAILURE) {
			return;
		}

		if (yaf_request_set_params_single(getThis(), name, len, value TSRMLS_CC)) {
			RETURN_ZVAL(self, 1, 0);
		}
	} else {
		WRONG_PARAM_COUNT;
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getParam(string $name, $mixed $default = NULL)
*	根据key从param中获取值并返回，如果没有查询到则返回设置的默认值
*/
PHP_METHOD(yaf_request, getParam) {
	char *name;
	uint len;
	zval *def = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &name, &len, &def) == FAILURE) {
		return;
	} else {
		zval *value = yaf_request_get_param(getThis(), name, len TSRMLS_CC);
		if (value) {
			RETURN_ZVAL(value, 1, 0);
		}
		if (def) {
			RETURN_ZVAL(def, 1, 0);
		}
	}

	RETURN_NULL();
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getException(void)
*/
PHP_METHOD(yaf_request, getException) {
	/* 获取$_exception */
	zval *exception = zend_read_property(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_EXCEPTION), 1 TSRMLS_CC);
	if (IS_OBJECT == Z_TYPE_P(exception)
			&& instanceof_function(Z_OBJCE_P(exception),
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
				zend_exception_get_default()
#else
				zend_exception_get_default(TSRMLS_C)
#endif
				TSRMLS_CC)) {
		RETURN_ZVAL(exception, 1, 0);
	}

	RETURN_NULL();
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getParams(void)
* 获取$params
*/
PHP_METHOD(yaf_request, getParams) {
	zval *params = zend_read_property(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), 1 TSRMLS_CC);
	RETURN_ZVAL(params, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getLanguage(void)
* 获取$language
*/
PHP_METHOD(yaf_request, getLanguage) {
	zval *lang = yaf_request_get_language(getThis() TSRMLS_CC);
	RETURN_ZVAL(lang, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getMethod(void)
* 获取$method
*/
PHP_METHOD(yaf_request, getMethod) {
	zval *method = yaf_request_get_method(getThis() TSRMLS_CC);
	RETURN_ZVAL(method, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::isDispatched(void)
* 获取$dispatched
*/
PHP_METHOD(yaf_request, isDispatched) {
	RETURN_BOOL(yaf_request_is_dispatched(getThis() TSRMLS_CC));
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::setDispatched(void)
* 设置$dispatched值为1
*/
PHP_METHOD(yaf_request, setDispatched) {
	RETURN_BOOL(yaf_request_set_dispatched(getThis(), 1 TSRMLS_CC));
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::setBaseUri(string $name)
*	手动设置$_base_uri，如果传入的不是合法的字符串的uri则返回false，base_uri设置成功后返回$this
*/
PHP_METHOD(yaf_request, setBaseUri) {
	zval *uri;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &uri) == FAILURE) {
		return;
	}

	if (Z_TYPE_P(uri) !=  IS_STRING || !Z_STRLEN_P(uri)) {
		RETURN_FALSE;
	}

	if (yaf_request_set_base_uri(getThis(), Z_STRVAL_P(uri), NULL TSRMLS_CC)) {
		RETURN_ZVAL(getThis(), 1, 0);
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getBaseUri(string $name)
*	获取base_uri
*/
PHP_METHOD(yaf_request, getBaseUri) {
	zval *uri = zend_read_property(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_BASE), 1 TSRMLS_CC);
	RETURN_ZVAL(uri, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::getRequestUri(string $name)
*	获取$uri
*/
PHP_METHOD(yaf_request, getRequestUri) {
	zval *uri = zend_read_property(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_URI), 1 TSRMLS_CC);
	RETURN_ZVAL(uri, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::setRequestUri(string $name)
*	设置$uri，成功后返回$this
*/
PHP_METHOD(yaf_request, setRequestUri) {
	char *uri;
	uint len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &uri, &len) == FAILURE) {
		return;
	}

	zend_update_property_stringl(yaf_request_ce, getThis(), ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_URI), uri, len TSRMLS_CC);
	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::isRouted(void)
*	判断$routed
*/
PHP_METHOD(yaf_request, isRouted) {
	RETURN_BOOL(yaf_request_is_routed(getThis() TSRMLS_CC));
}
/* }}} */

/** {{{ proto public Yaf_Request_Abstract::setRouted(void)
*	设置$routed的值为1，成功后返回$this
*/
PHP_METHOD(yaf_request, setRouted) {
	if (yaf_request_set_routed(getThis(),  1 TSRMLS_CC)) {
		RETURN_ZVAL(getThis(), 1, 0);
	}
	RETURN_FALSE;
}
/* }}} */

/** {{{ yaf_request_methods
*/
zend_function_entry yaf_request_methods[] = {
	PHP_ME(yaf_request, isGet, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, isPost,	yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, isPut, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, isHead, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, isOptions, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, isCli, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, isXmlHttpRequest, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getServer, yaf_request_getserver_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getEnv, yaf_request_getenv_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, setParam, yaf_request_set_param_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getParam, yaf_request_get_param_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getParams, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getException, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getModuleName, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getControllerName, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getActionName, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, setModuleName, yaf_request_set_module_name_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, setControllerName, yaf_request_set_controller_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, setActionName, yaf_request_set_action_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getMethod, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getLanguage, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, setBaseUri, yaf_request_set_baseuir_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getBaseUri,	yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, getRequestUri, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, setRequestUri, yaf_request_set_request_uri_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, isDispatched, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, setDispatched, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, isRouted, yaf_request_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request, setRouted, yaf_request_set_routed_arginfo, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
*/
YAF_STARTUP_FUNCTION(request){
	zend_class_entry ce;

	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Request_Abstract", "Yaf\\Request_Abstract", yaf_request_methods);
	yaf_request_ce 			= zend_register_internal_class_ex(&ce, NULL, NULL TSRMLS_CC);
	/* abstract class Yaf_Request_Abstract */
	yaf_request_ce->ce_flags = ZEND_ACC_EXPLICIT_ABSTRACT_CLASS;

	/**
	 *	public $module = null;
	 *	public $controller = null;
	 *	public $action = null;
	 *	public $method = null;
	 *
	 *	protected $params = null;
	 *	protected $language = null;
	 *	protected $_exception = null;
	 *
	 *	protected $_base_uri = "";
	 *	protected $uri = "";
	 *	protected $dispatched = 0;
	 *	protected $routed = 0;
	 */
	zend_declare_property_null(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE),     ZEND_ACC_PUBLIC	TSRMLS_CC);
	zend_declare_property_null(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION),     ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_METHOD),     ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS),  	ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_LANG), 		ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_EXCEPTION),  ZEND_ACC_PROTECTED TSRMLS_CC);

	zend_declare_property_string(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_BASE), "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_URI),  "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_STATE),	0,	ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_bool(yaf_request_ce, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ROUTED), 0, 	ZEND_ACC_PROTECTED TSRMLS_CC);

	YAF_STARTUP(request_http);
	YAF_STARTUP(request_simple);

	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
