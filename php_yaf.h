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

/* $Id: php_yaf.h 328979 2013-01-04 10:50:55Z laruence $ */

#ifndef PHP_YAF_H
#define PHP_YAF_H

extern zend_module_entry yaf_module_entry;
#define phpext_yaf_ptr &yaf_module_entry

#ifdef PHP_WIN32
#define PHP_YAF_API __declspec(dllexport)
#ifndef _MSC_VER
#define _MSC_VER 1600
#endif
#else
#define PHP_YAF_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#ifdef ZTS
#define YAF_G(v) TSRMG(yaf_globals_id, zend_yaf_globals *, v)
#else
#define YAF_G(v) (yaf_globals.v)
#endif

#define YAF_VERSION 					"2.2.9"

#define YAF_STARTUP_FUNCTION(module)   	ZEND_MINIT_FUNCTION(yaf_##module)
#define YAF_RINIT_FUNCTION(module)		ZEND_RINIT_FUNCTION(yaf_##module)
#define YAF_STARTUP(module)	 		  	ZEND_MODULE_STARTUP_N(yaf_##module)(INIT_FUNC_ARGS_PASSTHRU)
#define YAF_SHUTDOWN_FUNCTION(module)  	ZEND_MINIT_FUNCTION(yaf_##module)
#define YAF_SHUTDOWN(module)	 	    ZEND_MODULE_SHUTDOWN_N(yaf_##module)(INIT_FUNC_ARGS_PASSTHRU)

#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION > 2)) || (PHP_MAJOR_VERSION > 5)
#define YAF_HAVE_NAMESPACE
#else
#define Z_SET_REFCOUNT_P(pz, rc)      (pz)->refcount = rc
#define Z_SET_REFCOUNT_PP(ppz, rc)    Z_SET_REFCOUNT_P(*(ppz), rc)
#define Z_ADDREF_P 	 ZVAL_ADDREF
#define Z_REFCOUNT_P ZVAL_REFCOUNT
#define Z_DELREF_P 	 ZVAL_DELREF
#endif

#define yaf_application_t	zval
#define yaf_view_t 			zval
#define yaf_controller_t	zval
#define yaf_request_t		zval
#define yaf_router_t		zval
#define yaf_route_t			zval
#define yaf_dispatcher_t	zval
#define yaf_action_t		zval
#define yaf_loader_t		zval
#define yaf_response_t		zval
#define yaf_config_t		zval
#define yaf_registry_t		zval
#define yaf_plugin_t		zval
#define yaf_session_t		zval
#define yaf_exception_t		zval

#define YAF_ME(c, m, a, f) {m, PHP_MN(c), a, (zend_uint) (sizeof(a)/sizeof(struct _zend_arg_info)-1), f},

extern PHPAPI void php_var_dump(zval **struc, int level TSRMLS_DC);
extern PHPAPI void php_debug_zval_dump(zval **struc, int level TSRMLS_DC);

ZEND_BEGIN_MODULE_GLOBALS(yaf)
	char 		*ext;                       //脚本后缀
	char		*base_uri;                  //base_uri
	char 		*environ;                   //环境名称, 当用INI作为Yaf的配置文件时, 这个指明了Yaf将要在INI配置中读取的节的名字
	char 		*directory;                 //application目录路径
	char 		*local_library;             //本地类库      
	char        *local_namespaces;          //本地类前缀(字符串形式，类似：:Foo:Bar:)
	char 		*global_library;            //全局类库
	char        *view_directory;            //视图文件目录路径
	char 		*view_ext;                  //视图文件后缀
	char 		*default_module;            //默认module
	char 		*default_controller;        //默认controller
	char 		*default_action;            //默认action
	char 		*bootstrap;                 //bootstrap路径
	char 		*name_separator;            //类名分隔符
	long 		name_separator_len;         //类名分隔符长度
	zend_bool 	lowcase_path;               //开启的情况下，路径信息中的目录部分都会被转换成小写
	zend_bool 	use_spl_autoload;           //开启的情况下，Yaf在加载不成功的情况下，会继续让PHP的自动加载函数加载，从性能考虑，除非特殊情况，否则保持这个选项关闭
	zend_bool 	throw_exception;
	zend_bool 	cache_config;               //是否缓存配置文件(只针对INI配置文件生效)，打开此选项可在复杂配置的情况下提高性能
	zend_bool   action_prefer;              //当PATH_INFO只有一部分的时候，将其作为controller还是action，配置打开则为action
	zend_bool	name_suffix;                 //在处理Controller, Action, Plugin, Model的时候, 类名中关键信息是否是后缀式,比如UserModel, 而在前缀模式下则是ModelUser
	zend_bool  	autoload_started;
	zend_bool  	running;
	zend_bool  	in_exception;
	zend_bool  	catch_exception;
	zend_bool   suppressing_warning;
/* {{{ This only effects internally */
	zend_bool  	st_compatible;             //打开后到model下找相关Dao和Service
/* }}} */
	long		forward_limit;           //forward最大嵌套深度
	HashTable	*configs;                //配置缓存
	zval 		*modules;
	zval        *default_route;            //默认路由
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 4))
	uint 		buf_nesting;
	void		*buffer;
	void 		*owrite_handler;
#endif
	zval        *active_ini_file_section;      //TODO ini配置文件活跃的单节，类似[common]
	zval        *ini_wanted_section;
	uint        parsing_flag;          //配置文件解析进度标识
#ifdef YAF_HAVE_NAMESPACE
	zend_bool	use_namespace;           //开启的情况下, Yaf将会使用命名空间方式注册自己的类, 比如Yaf_Application将会变成Yaf\Application
#endif
ZEND_END_MODULE_GLOBALS(yaf)

PHP_MINIT_FUNCTION(yaf);
PHP_MSHUTDOWN_FUNCTION(yaf);
PHP_RINIT_FUNCTION(yaf);
PHP_RSHUTDOWN_FUNCTION(yaf);
PHP_MINFO_FUNCTION(yaf);

extern ZEND_DECLARE_MODULE_GLOBALS(yaf);

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
