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

/* $Id: yaf_application.c 328604 2012-12-02 05:47:27Z laruence $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "Zend/zend_interfaces.h"
#include "ext/standard/php_var.h"
#include "ext/standard/basic_functions.h"

#include "php_yaf.h"
#include "yaf_namespace.h"
#include "yaf_application.h"
#include "yaf_dispatcher.h"
#include "yaf_router.h"
#include "yaf_config.h"
#include "yaf_loader.h"
#include "yaf_request.h"
#include "yaf_bootstrap.h"
#include "yaf_exception.h"

zend_class_entry * yaf_application_ce;

/** {{{ ARG_INFO
 *  */
ZEND_BEGIN_ARG_INFO_EX(yaf_application_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_construct_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, config)
	ZEND_ARG_INFO(0, envrion)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_app_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_execute_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, entry)
	ZEND_ARG_INFO(0, ...)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_getconfig_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_getmodule_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_getdispatch_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_bootstrap_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, bootstrap)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_environ_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_run_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_application_setappdir_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, directory)
ZEND_END_ARG_INFO()
/* }}} */

/** {{{ int yaf_application_is_module_name(char *name, int len TSRMLS_DC)
 *	判断名称是否是已经注册了的module的名称
*/
int yaf_application_is_module_name(char *name, int len TSRMLS_DC) {
	zval 				*modules, **ppzval;
	HashTable 			*ht;
	yaf_application_t 	*app;
	/* 获取类的实例，$app = self::$_app */
	app = zend_read_static_property(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_APP), 1 TSRMLS_CC);
	if (!app || Z_TYPE_P(app) != IS_OBJECT) {
		return 0;
	}
	/* $modules = $this->_modules */
	modules = zend_read_property(yaf_application_ce, app, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_MODULES), 1 TSRMLS_CC);
	if (!modules || Z_TYPE_P(modules) != IS_ARRAY) {
		return 0;
	}
	/* 检测name是否在$this->_modules数组中，在就返回1，不在返回0 */
	ht = Z_ARRVAL_P(modules);
	zend_hash_internal_pointer_reset(ht);
	while (zend_hash_get_current_data(ht, (void **)&ppzval) == SUCCESS) {
		if (Z_TYPE_PP(ppzval) == IS_STRING && Z_STRLEN_PP(ppzval) == len
				&& strncasecmp(Z_STRVAL_PP(ppzval), name, len) == 0) {
			return 1;
		}
		zend_hash_move_forward(ht);
	}
	return 0;
}
/* }}} */

/** {{{ static int yaf_application_parse_option(zval *options TSRMLS_DC)
 *	根据配置文件解析得到的数组，进行一些列的配置项的赋值以及初始化工作
 */
static int yaf_application_parse_option(zval *options TSRMLS_DC) {
	HashTable 	*conf;
	zval  		**ppzval, **ppsval, *app;

	/* 1.验证配置文件是否包含框架需要的配置信息 */
	conf = HASH_OF(options);
	if (zend_hash_find(conf, ZEND_STRS("application"), (void **)&ppzval) == FAILURE) {
		/* For back compatibilty */
		if (zend_hash_find(conf, ZEND_STRS("yaf"), (void **)&ppzval) == FAILURE) {
			yaf_trigger_error(YAF_ERR_TYPE_ERROR TSRMLS_CC, "%s", "Expected an array of application configure");
			return FAILURE;
		}
	}

	app = *ppzval;
	if (Z_TYPE_P(app) != IS_ARRAY) {
		yaf_trigger_error(YAF_ERR_TYPE_ERROR TSRMLS_CC, "%s", "Expected an array of application configure");
		return FAILURE;
	}
	/* 2.检验配置文件中 application.directory */
	if (zend_hash_find(Z_ARRVAL_P(app), ZEND_STRS("directory"), (void **)&ppzval) == FAILURE
			|| Z_TYPE_PP(ppzval) != IS_STRING) {
		yaf_trigger_error(YAF_ERR_STARTUP_FAILED TSRMLS_CC, "%s", "Expected a directory entry in application configures");
		return FAILURE;
	}
	/* 3.初始化全局变量directory，将它设置为配置文件中application.directory对应的值 */
	if (*(Z_STRVAL_PP(ppzval) + Z_STRLEN_PP(ppzval) - 1) == DEFAULT_SLASH) {
		YAF_G(directory) = estrndup(Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval) - 1);
	} else {
		YAF_G(directory) = estrndup(Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval));
	}
	/* 4.验证在配置文件中是否设置ext,设置就使用用户设置，没设置则使用默认的"php" */
	if (zend_hash_find(Z_ARRVAL_P(app), ZEND_STRS("ext"), (void **)&ppzval) == SUCCESS
			&& Z_TYPE_PP(ppzval) == IS_STRING) {
		YAF_G(ext) = estrndup(Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval));
	} else {
		YAF_G(ext) = YAF_DEFAULT_EXT;
	}
	/* 5.配置文件中设置了bootstrap的路径（application.bootstrap = APPLICATION_PATH"/Bootstrap.php"），则将这个信息写入全局变量bootstrap中 */
	if (zend_hash_find(Z_ARRVAL_P(app), ZEND_STRS("bootstrap"), (void **)&ppzval) == SUCCESS
			&& Z_TYPE_PP(ppzval) == IS_STRING) {
		YAF_G(bootstrap) = estrndup(Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval));
	}
	/* 6.判断配置文件中是否设置了 application.library */
	if (zend_hash_find(Z_ARRVAL_P(app), ZEND_STRS("library"), (void **)&ppzval) == SUCCESS) {
		if (IS_STRING == Z_TYPE_PP(ppzval)) {
			/* 6.1.配置文件设置了application.library并且值为字符串，则设置本地类库地址为这里设置的配置文件的值 */
			YAF_G(local_library) = estrndup(Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval));
		} else if (IS_ARRAY == Z_TYPE_PP(ppzval)) {
			/* 6.2.配置文件设置了application.library，并且为值为数组 */

			if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("directory"), (void **)&ppsval) == SUCCESS
					&& Z_TYPE_PP(ppsval) == IS_STRING) {
				/* application.library.directory存在并且值为字符串，则将本地类库地址设置为这个key对应的值 */
				YAF_G(local_library) = estrndup(Z_STRVAL_PP(ppsval), Z_STRLEN_PP(ppsval));
			}
			if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("namespace"), (void **)&ppsval) == SUCCESS
					&& Z_TYPE_PP(ppsval) == IS_STRING) {
				/** 
				 *	6.3.application.library.namespace存在并且值为字符串 
				 *
				 *	转换并且注册namespace：
				 *	1.形式：application.library.namespace = "namespace1,namespace2.namespce3"
				 *	2.转换后的形式：namespace1:namespace2:namespce3
				 *	3.然后注册到local_namespaces本地类前缀(字符串形式，类似：:Foo:Bar:)
				 */				
				uint i, len;
				char *src = Z_STRVAL_PP(ppsval);
				if (Z_STRLEN_PP(ppsval)) {
				    char *target = emalloc(Z_STRLEN_PP(ppsval) + 1);
					len = 0;
					for(i=0; i<Z_STRLEN_PP(ppsval); i++) {
						if (src[i] == ',') {
							target[len++] = DEFAULT_DIR_SEPARATOR;
						} else if (src[i] != ' ') {
                            target[len++] = src[i];
						}
					}
					target[len] = '\0';
					yaf_loader_register_namespace_single(target, len TSRMLS_CC);
					efree(target);
				}
			}
		}
	}
	/** 
	 *	7.
	 *		1.先找application.view，如果不存在或者值不为数组（不为application.view.ext = "phtml"）的话，则使用默认值“phtml” 作为view文件名后缀
	 *		2.application.view存在且为数组，在这个数组里面查找ext，不存在则使用默认值“phtml”作为view文件名后缀
	 *		3.application.view存在且为数组，并且这个值数组中存在ext，则使用ext所指定的值作为view文件名后缀
	 */
	if (zend_hash_find(Z_ARRVAL_P(app), ZEND_STRS("view"), (void **)&ppzval) == FAILURE 
			|| Z_TYPE_PP(ppzval) != IS_ARRAY) {
		YAF_G(view_ext) = YAF_DEFAULT_VIEW_EXT;
	} else {
		if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("ext"), (void **)&ppsval) == FAILURE
				|| Z_TYPE_PP(ppsval) != IS_STRING) {
			YAF_G(view_ext) = YAF_DEFAULT_VIEW_EXT;
		} else {
			YAF_G(view_ext) = estrndup(Z_STRVAL_PP(ppsval), Z_STRLEN_PP(ppsval));
		}
	}
	/* 8.存在application.baseUri则设置YAF_G(base_uri)为其值 */
	if (zend_hash_find(Z_ARRVAL_P(app), ZEND_STRS("baseUri"), (void **)&ppzval) == SUCCESS
			&& Z_TYPE_PP(ppzval) == IS_STRING) {
		YAF_G(base_uri) = estrndup(Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval));
	}
	/* 9.进行application.dispatcher的判断 */
	if (zend_hash_find(Z_ARRVAL_P(app), ZEND_STRS("dispatcher"), (void **)&ppzval) == FAILURE
			|| Z_TYPE_PP(ppzval) != IS_ARRAY) {
		/* 1.没设置application.dispatcher或者application.dispatcher下面的值不是数组的话，则default_module、default_controller、default_action都使用系统默认值 */
		YAF_G(default_module) = YAF_ROUTER_DEFAULT_MODULE;
		YAF_G(default_controller) = YAF_ROUTER_DEFAULT_CONTROLLER;
		YAF_G(default_action)  = YAF_ROUTER_DEFAULT_ACTION;
	} else {
		/* 2.没设置application.dispatcher并且application.dispatcher下面的值是数组 */

		/** 
		 *	2.1.没设置application.dispatcher.defaultModule，或者它的值不为字符串，则使用默认值给YAF_G(default_module)赋值 
		 *		相反的，则将application.dispatcher.defaultModule对应的值转换成小写后传递给YAF_G(default_module)
		 *			然后再将YAF_G(default_module)转换成全部大写
		 */
		if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("defaultModule"), (void **)&ppsval) == FAILURE
				|| Z_TYPE_PP(ppsval) != IS_STRING) {
			YAF_G(default_module) = YAF_ROUTER_DEFAULT_MODULE;
		} else {
			YAF_G(default_module) = zend_str_tolower_dup(Z_STRVAL_PP(ppsval), Z_STRLEN_PP(ppsval));
			*(YAF_G(default_module)) = toupper(*YAF_G(default_module));
		}

		/** 
		 *	2.2.没设置application.dispatcher.defaultController，或者它的值不为字符串，则使用默认值给YAF_G(default_controller)赋值 
		 *		相反的，则将application.dispatcher.defaultController对应的值转换成小写后传递给YAF_G(default_controller)
		 *			然后再将YAF_G(default_controller)转换成全部大写
		 */
		if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("defaultController"), (void **)&ppsval) == FAILURE
				|| Z_TYPE_PP(ppsval) != IS_STRING) {
			YAF_G(default_controller) = YAF_ROUTER_DEFAULT_CONTROLLER;
		} else {
			YAF_G(default_controller) = zend_str_tolower_dup(Z_STRVAL_PP(ppsval), Z_STRLEN_PP(ppsval));
			*(YAF_G(default_controller)) = toupper(*YAF_G(default_controller));
		}

		/** 
		 *	2.2.没设置application.dispatcher.defaultAction，或者它的值不为字符串，则使用默认值给YAF_G(default_action)赋值 
		 *		相反的，则将application.dispatcher.defaultAction对应的值转换成小写后传递给YAF_G(default_action)
		 */
		if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("defaultAction"), (void **)&ppsval) == FAILURE
				|| Z_TYPE_PP(ppsval) != IS_STRING) {
			YAF_G(default_action)	  = YAF_ROUTER_DEFAULT_ACTION;
		} else {
			YAF_G(default_action) = zend_str_tolower_dup(Z_STRVAL_PP(ppsval), Z_STRLEN_PP(ppsval));
		}

		/* 2.3.检验是否设置application.dispatcher.throwException */
		if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("throwException"), (void **)&ppsval) == SUCCESS) {
			/* 先refcount__gc++,然后转换成boolean，然后赋值 */
			zval *tmp = *ppsval;
			Z_ADDREF_P(tmp);
			convert_to_boolean_ex(&tmp);
			YAF_G(throw_exception) = Z_BVAL_P(tmp);
			zval_ptr_dtor(&tmp);
		}
		/* 2.4.检验是否设置application.dispatcher.catchException */
		if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("catchException"), (void **)&ppsval) == SUCCESS) {
			/* 先refcount__gc++,然后转换成boolean，然后赋值 */
			zval *tmp = *ppsval;
			Z_ADDREF_P(tmp);
			convert_to_boolean_ex(&tmp);
			YAF_G(catch_exception) = Z_BVAL_P(tmp);
			zval_ptr_dtor(&tmp);
		}
		/* 2.5.检验是否设置application.dispatcher.defaultRoute，设置了就将它赋值给YAF_G(default_route) */
		if (zend_hash_find(Z_ARRVAL_PP(ppzval), ZEND_STRS("defaultRoute"), (void **)&ppsval) == SUCCESS
				&& Z_TYPE_PP(ppsval) == IS_ARRAY) {
			YAF_G(default_route) = *ppsval;
		}
	}

	/* 10.设置application.modules */
	do {
		char *ptrptr;
		zval *module, *zmodules;

		MAKE_STD_ZVAL(zmodules);
		array_init(zmodules);
		if (zend_hash_find(Z_ARRVAL_P(app), ZEND_STRS("modules"), (void **)&ppzval) == SUCCESS
				&& Z_TYPE_PP(ppzval) == IS_STRING && Z_STRLEN_PP(ppzval)) {
			/* 已经设置application.modules，并且值为合法字符串 */
			char *seg, *modules;
			modules = estrndup(Z_STRVAL_PP(ppzval), Z_STRLEN_PP(ppzval));

			/* 以','分割modules字符串，并将分割出来的值放入数组zmodules中 */
			seg = php_strtok_r(modules, ",", &ptrptr);	
			while(seg) {
				if (seg && strlen(seg)) {
					MAKE_STD_ZVAL(module);
					ZVAL_STRINGL(module, seg, strlen(seg), 1);
					zend_hash_next_index_insert(Z_ARRVAL_P(zmodules),
							(void **)&module, sizeof(zval *), NULL);
				}
				seg = php_strtok_r(NULL, ",", &ptrptr);
			}
			efree(modules);
		} else {
			/* 没设置application.modules，或者值不为合法字符串，则将YAF_G(default_module)作为一个元素加入到zmodules数组中 */
			MAKE_STD_ZVAL(module);
			ZVAL_STRING(module, YAF_G(default_module), 1);
			zend_hash_next_index_insert(Z_ARRVAL_P(zmodules), (void **)&module, sizeof(zval *), NULL);
		}
		/* 将得到的modules数组赋值给YAF_G(modules) */
		YAF_G(modules) = zmodules;
	} while (0);

	/* 11.设置application.system */
	/**
	 *	application.system.*
	 *	通过这个属性, 可以修改yaf的runtime configure, 比如application.system.lowcase_path, 
	 *	但是请注意只有PHP_INI_ALL的配置项才可以在这里被修改, 此选项从2.2.0开始引入
	 */
	if (zend_hash_find(Z_ARRVAL_P(app), ZEND_STRS("system"), (void **)&ppzval) == SUCCESS && Z_TYPE_PP(ppzval) == IS_ARRAY) {
		/* application.system在配置文件中存在，并且值为数组 */
		zval **value;
		char *key, name[128];
		HashTable *ht = Z_ARRVAL_PP(ppzval);
		/* 循环遍历application.system数组 */
		for(zend_hash_internal_pointer_reset(ht);
				zend_hash_has_more_elements(ht) == SUCCESS;
				zend_hash_move_forward(ht)) {
			uint len;
			ulong idx;
			/* application.system.xxx 这里的xxx只能为字符串 */
			if (zend_hash_get_current_key_ex(ht, &key, &len, &idx, 0, NULL) != HASH_KEY_IS_STRING) {
				continue;
			}

			if (zend_hash_get_current_data(ht, (void **)&value) == FAILURE) {
				continue;
			}
			/* $name = 'yaf.xxx' */
			len = snprintf(name, sizeof(name), "%s.%s", "yaf", key);
			if (len > sizeof(name) -1) {
				len = sizeof(name) - 1;
			}
			convert_to_string(*value);	/* 将对应的值转换为字符串类型 */
			/* 进行运行是修改yaf的configure */
			zend_alter_ini_entry(name, len + 1, Z_STRVAL_PP(value), Z_STRLEN_PP(value), PHP_INI_USER, PHP_INI_STAGE_RUNTIME);
		}
	}

	return SUCCESS;
}
/* }}} */

/** {{{ proto Yaf_Application::__construct(mixed $config, string $environ = YAF_G(environ))
*/
PHP_METHOD(yaf_application, __construct) {
	yaf_config_t 	 	*zconfig;
	yaf_request_t 	 	*request;
	yaf_dispatcher_t	*zdispatcher;
	yaf_application_t	*app, *self;
	yaf_loader_t		*loader;
	zval 				*config;
	zval 				*section = NULL;
	/* $app = selef::$_app */
	app	 = zend_read_static_property(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_APP), 1 TSRMLS_CC);

#if PHP_YAF_DEBUG
	php_error_docref(NULL, E_STRICT, "Yaf is running in debug mode");
#endif

	if (!ZVAL_IS_NULL(app)) {
		yaf_trigger_error(YAF_ERR_STARTUP_FAILED TSRMLS_CC, "Only one application can be initialized");
		RETURN_FALSE;
	}

	self = getThis();
	/* 接收配置信息 */
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &config, &section) == FAILURE) {
		YAF_UNINITIALIZED_OBJECT(getThis());
		return;
	}

	if (!section || Z_TYPE_P(section) != IS_STRING || !Z_STRLEN_P(section)) {
		/* 没有传入要读取的节的名称，使用默认节名 */
		MAKE_STD_ZVAL(section);
		ZVAL_STRING(section, YAF_G(environ), 0);
		/* 实例化Yaf_Config，得到对象 */
		zconfig = yaf_config_instance(NULL, config, section TSRMLS_CC);
		efree(section);
	} else {
		/* 传入节信息了，实例化Yaf_Config，得到对象 */
		zconfig = yaf_config_instance(NULL, config, section TSRMLS_CC);
	}

	/* 类实例化失败，以及配置文件解析失败报错 */
	if  (zconfig == NULL
			|| Z_TYPE_P(zconfig) != IS_OBJECT
			|| !instanceof_function(Z_OBJCE_P(zconfig), yaf_config_ce TSRMLS_CC)
			|| yaf_application_parse_option(zend_read_property(yaf_config_ce,
				   	zconfig, ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC) TSRMLS_CC) == FAILURE) {
		YAF_UNINITIALIZED_OBJECT(getThis());
		yaf_trigger_error(YAF_ERR_STARTUP_FAILED TSRMLS_CC, "Initialization of application config failed");
		RETURN_FALSE;
	}
	/* 实例化Yaf_Request类 */
	request = yaf_request_instance(NULL, YAF_G(base_uri) TSRMLS_CC);
	if (YAF_G(base_uri)) {
		efree(YAF_G(base_uri));
		YAF_G(base_uri) = NULL;
	}
	/* 实例化Yaf_Request失败报错 */
	if (!request) {
		YAF_UNINITIALIZED_OBJECT(getThis());
		yaf_trigger_error(YAF_ERR_STARTUP_FAILED TSRMLS_CC, "Initialization of request failed");
		RETURN_FALSE;
	}
	/* 实例化Yaf_Dispatcher，失败就报错 */
	zdispatcher = yaf_dispatcher_instance(NULL TSRMLS_CC);
	if (NULL == zdispatcher
			|| Z_TYPE_P(zdispatcher) != IS_OBJECT
			|| !instanceof_function(Z_OBJCE_P(zdispatcher), yaf_dispatcher_ce TSRMLS_CC)) {
		YAF_UNINITIALIZED_OBJECT(getThis());
		yaf_trigger_error(YAF_ERR_STARTUP_FAILED TSRMLS_CC, "Instantiation of application dispatcher failed");
		RETURN_FALSE;
	}
	/* 给Yaf_Dispatcher的实例传递Yaf_Request的实例 */
	yaf_dispatcher_set_request(zdispatcher, request TSRMLS_CC);
	/**
	 *	$this->config = new Yaf_Config();
	 *	$this->dispatcher = new Yaf_Dispatcher();
	 */
	zend_update_property(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_CONFIG), zconfig TSRMLS_CC);
	zend_update_property(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_DISPATCHER), zdispatcher TSRMLS_CC);

	zval_ptr_dtor(&request);
	zval_ptr_dtor(&zdispatcher);
	zval_ptr_dtor(&zconfig);

	if (YAF_G(local_library)) {
		/* 设置了本地类库路径，则利用本地类库路径和全局类库路径实例化Yaf_Loader类 */
		loader = yaf_loader_instance(NULL, YAF_G(local_library),
				strlen(YAF_G(global_library))? YAF_G(global_library) : NULL TSRMLS_CC);
		efree(YAF_G(local_library));
		YAF_G(local_library) = NULL;
	} else {
		/** 
		 *	没设置本地类库路劲的情况下，则利用application.directory以及默认分隔符还有默认类库文件夹名生成一个本地类库路径
		 *	然后再结合全局类库名来实例化Yaf_Loader类
		 */
		char *local_library;
		spprintf(&local_library, 0, "%s%c%s", YAF_G(directory), DEFAULT_SLASH, YAF_LIBRARY_DIRECTORY_NAME);
		loader = yaf_loader_instance(NULL, local_library,
				strlen(YAF_G(global_library))? YAF_G(global_library) : NULL TSRMLS_CC);
		efree(local_library);
	}
	/* Yaf_Loader实例化失败报错 */
	if (!loader) {
		YAF_UNINITIALIZED_OBJECT(getThis());
		yaf_trigger_error(YAF_ERR_STARTUP_FAILED TSRMLS_CC, "Initialization of application auto loader failed");
		RETURN_FALSE;
	}

	/**
	 *	$this->_running = false;
	 *	$this->_environ = YAF_G(environ)
	 */
	zend_update_property_bool(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_RUN), 0 TSRMLS_CC);
	zend_update_property_string(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_ENV), YAF_G(environ) TSRMLS_CC);
	/* 设置了YAF_G(modules)的情况下，$this->_modules=YAF_G(modules)，否则$this->_modules=null */
	if (YAF_G(modules)) {
		zend_update_property(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_MODULES), YAF_G(modules) TSRMLS_CC);
		Z_DELREF_P(YAF_G(modules));
		YAF_G(modules) = NULL;
	} else {
		zend_update_property_null(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_MODULES) TSRMLS_CC);
	}
	/* Yaf_Application::$_app = new Yaf_Application() */
	zend_update_static_property(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_APP), self TSRMLS_CC);
}
/* }}} */

/** {{{ proto public Yaf_Application::__desctruct(void)
*/
PHP_METHOD(yaf_application, __destruct) {
	/* self::$_app = null */
   zend_update_static_property_null(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_APP) TSRMLS_CC);
}
/* }}} */

/** {{{ proto private Yaf_Application::__sleep(void)
*/
PHP_METHOD(yaf_application, __sleep) {
}
/* }}} */

/** {{{ proto private Yaf_Application::__wakeup(void)
*/
PHP_METHOD(yaf_application, __wakeup) {
}
/* }}} */

/** {{{ proto private Yaf_Application::__clone(void)
*/
PHP_METHOD(yaf_application, __clone) {
}
/* }}} */

/** {{{ proto public Yaf_Application::run(void)
*/
PHP_METHOD(yaf_application, run) {
	zval *running;
	yaf_dispatcher_t  *dispatcher;
	yaf_response_t	  *response;
	yaf_application_t *self = getThis();
	/* $running = $this->_running */
	running = zend_read_property(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_RUN), 1 TSRMLS_CC);
	/* if ($running === true) {报错}*/
	if (IS_BOOL == Z_TYPE_P(running)
			&& Z_BVAL_P(running)) {
		yaf_trigger_error(YAF_ERR_STARTUP_FAILED TSRMLS_CC, "An application instance already run");
		RETURN_TRUE;
	}
	/* $running = true */
	ZVAL_BOOL(running, 1);
	/* $this->_running = $running */
	zend_update_property(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_RUN), running TSRMLS_CC);
	/* $dispatcher = $this->dispatcher */
	dispatcher = zend_read_property(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_DISPATCHER), 1 TSRMLS_CC);
	if ((response = yaf_dispatcher_dispatch(dispatcher TSRMLS_CC))) {
		RETURN_ZVAL(response, 1, 1);
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_Application::execute(callback $func)
 * We can not call to zif_call_user_func on windows, since it was not declared with dllexport
*/
PHP_METHOD(yaf_application, execute) {
#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION > 2)) || (PHP_MAJOR_VERSION > 5)
    zval *retval_ptr;
    zend_fcall_info fci;
    zend_fcall_info_cache fci_cache;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "f*", &fci, &fci_cache, &fci.params, &fci.param_count) == FAILURE) {
        return;
    }

    fci.retval_ptr_ptr = &retval_ptr;

    if (zend_call_function(&fci, &fci_cache TSRMLS_CC) == SUCCESS && fci.retval_ptr_ptr && *fci.retval_ptr_ptr) {
        COPY_PZVAL_TO_ZVAL(*return_value, *fci.retval_ptr_ptr);
    }

    if (fci.params) {
        efree(fci.params);
    }
#else
    zval ***params;
    zval *retval_ptr;
    char *name;
    int argc = ZEND_NUM_ARGS();

    if (argc < 1) {
        return;
    }

    params = safe_emalloc(sizeof(zval **), argc, 0);
    if (zend_get_parameters_array_ex(1, params) == FAILURE) {
        efree(params);
        RETURN_FALSE;
    }

    if (Z_TYPE_PP(params[0]) != IS_STRING && Z_TYPE_PP(params[0]) != IS_ARRAY) {
        SEPARATE_ZVAL(params[0]);
        convert_to_string_ex(params[0]);
    }

    if (!zend_is_callable(*params[0], 0, &name)) {
        php_error_docref1(NULL TSRMLS_CC, name, E_WARNING, "First argument is expected to be a valid callback");
        efree(name);
        efree(params);
        RETURN_NULL();
    }

    if (zend_get_parameters_array_ex(argc, params) == FAILURE) {
        efree(params);
        RETURN_FALSE;
    }

    if (call_user_function_ex(EG(function_table), NULL, *params[0], &retval_ptr, argc - 1, params + 1, 0, NULL TSRMLS_CC) == SUCCESS) {
        if (retval_ptr) {
            COPY_PZVAL_TO_ZVAL(*return_value, retval_ptr);
        }
    } else {
        if (argc > 1) {
            SEPARATE_ZVAL(params[1]);
            convert_to_string_ex(params[1]);
            if (argc > 2) {
                SEPARATE_ZVAL(params[2]);
                convert_to_string_ex(params[2]);
                php_error_docref1(NULL TSRMLS_CC, name, E_WARNING, "Unable to call %s(%s,%s)", name, Z_STRVAL_PP(params[1]), Z_STRVAL_PP(params[2]));
            } else {
                php_error_docref1(NULL TSRMLS_CC, name, E_WARNING, "Unable to call %s(%s)", name, Z_STRVAL_PP(params[1]));
            }
        } else {
            php_error_docref1(NULL TSRMLS_CC, name, E_WARNING, "Unable to call %s()", name);
        }
    }

    efree(name);
    efree(params);
#endif
}
/* }}} */

/** {{{ proto public Yaf_Application::app(void)
*/
PHP_METHOD(yaf_application, app) {
	yaf_application_t *app = zend_read_static_property(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_APP), 1 TSRMLS_CC);
	RETVAL_ZVAL(app, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Application::getConfig(void)
*/
PHP_METHOD(yaf_application, getConfig) {
	yaf_config_t *config = zend_read_property(yaf_application_ce, getThis(), ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_CONFIG), 1 TSRMLS_CC);
	RETVAL_ZVAL(config, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Application::getDispatcher(void)
*/
PHP_METHOD(yaf_application, getDispatcher) {
	yaf_dispatcher_t *dispatcher = zend_read_property(yaf_application_ce, getThis(), ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_DISPATCHER), 1 TSRMLS_CC);
	RETVAL_ZVAL(dispatcher, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Application::getModules(void)
*/
PHP_METHOD(yaf_application, getModules) {
	zval *modules = zend_read_property(yaf_application_ce, getThis(), ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_MODULES), 1 TSRMLS_CC);
	RETVAL_ZVAL(modules, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Application::environ(void)
*/
PHP_METHOD(yaf_application, environ) {
	zval *env = zend_read_property(yaf_application_ce, getThis(), ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_ENV), 1 TSRMLS_CC);
	RETURN_STRINGL(Z_STRVAL_P(env), Z_STRLEN_P(env), 1);
}
/* }}} */

/** {{{ proto public Yaf_Application::bootstrap(void)
*/
PHP_METHOD(yaf_application, bootstrap) {
	char *bootstrap_path;
	uint len, retval = 1;
	zend_class_entry	**ce;
	yaf_application_t	*self = getThis();

	if (zend_hash_find(EG(class_table), YAF_DEFAULT_BOOTSTRAP_LOWER, YAF_DEFAULT_BOOTSTRAP_LEN, (void **) &ce) != SUCCESS) {
		if (YAF_G(bootstrap)) {
			bootstrap_path  = estrdup(YAF_G(bootstrap));
			len = strlen(YAF_G(bootstrap));
		} else {
			len = spprintf(&bootstrap_path, 0, "%s%c%s.%s", YAF_G(directory), DEFAULT_SLASH, YAF_DEFAULT_BOOTSTRAP, YAF_G(ext));
		}

		if (!yaf_loader_import(bootstrap_path, len + 1, 0 TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't find bootstrap file %s", bootstrap_path);
			retval = 0;
		} else if (zend_hash_find(EG(class_table), YAF_DEFAULT_BOOTSTRAP_LOWER, YAF_DEFAULT_BOOTSTRAP_LEN, (void **) &ce) != SUCCESS)  {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't find class %s in %s", YAF_DEFAULT_BOOTSTRAP, bootstrap_path);
			retval = 0;
		} else if (!instanceof_function(*ce, yaf_bootstrap_ce TSRMLS_CC)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expect a %s instance, %s give", yaf_bootstrap_ce->name, (*ce)->name);
			retval = 0;
		}

		efree(bootstrap_path);
	}

	if (!retval) {
		RETURN_FALSE;
	} else {
		zval 			*bootstrap;
		HashTable 		*methods;
		yaf_dispatcher_t *dispatcher;

		MAKE_STD_ZVAL(bootstrap);
		object_init_ex(bootstrap, *ce);
		dispatcher 	= zend_read_property(yaf_application_ce, self, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_DISPATCHER), 1 TSRMLS_CC);

		methods		= &((*ce)->function_table);
		for(zend_hash_internal_pointer_reset(methods);
				zend_hash_has_more_elements(methods) == SUCCESS;
				zend_hash_move_forward(methods)) {
			char *func;
			uint len;
			ulong idx;

			zend_hash_get_current_key_ex(methods, &func, &len, &idx, 0, NULL);
			/* cann't use ZEND_STRL in strncasecmp, it cause a compile failed in VS2009 */
			if (strncasecmp(func, YAF_BOOTSTRAP_INITFUNC_PREFIX, sizeof(YAF_BOOTSTRAP_INITFUNC_PREFIX)-1)) {
				continue;
			}

			zend_call_method(&bootstrap, *ce, NULL, func, len - 1, NULL, 1, dispatcher, NULL TSRMLS_CC);
			/** an uncaught exception threw in function call */
			if (EG(exception)) {
				zval_ptr_dtor(&bootstrap);
				RETURN_FALSE;
			}
		}

		zval_ptr_dtor(&bootstrap);
	}

	RETVAL_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Application::getLastErrorNo(void)
*/
PHP_METHOD(yaf_application, getLastErrorNo) {
	zval *errcode = zend_read_property(yaf_application_ce, getThis(), ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_ERRNO), 1 TSRMLS_CC);
	RETURN_LONG(Z_LVAL_P(errcode));
}
/* }}} */

/** {{{ proto public Yaf_Application::getLastErrorMsg(void)
*/
PHP_METHOD(yaf_application, getLastErrorMsg) {
	zval *errmsg = zend_read_property(yaf_application_ce, getThis(), ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_ERRMSG), 1 TSRMLS_CC);
	RETURN_STRINGL(Z_STRVAL_P(errmsg), Z_STRLEN_P(errmsg), 1);
}
/* }}} */

/** {{{ proto public Yaf_Application::clearLastError(void)
*/
PHP_METHOD(yaf_application, clearLastError) {
	zend_update_property_long(yaf_application_ce, getThis(), ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_ERRNO), 0 TSRMLS_CC);
	zend_update_property_string(yaf_application_ce, getThis(), ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_ERRMSG), "" TSRMLS_CC);

	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Application::setAppDirectory(string $directory)
*/
PHP_METHOD(yaf_application, setAppDirectory) {
	int	 len;
	char *directory;
	yaf_dispatcher_t *self = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &directory, &len) == FAILURE) {
		return;
	}

	if (!len || !IS_ABSOLUTE_PATH(directory, len)) {
		RETURN_FALSE;
	}

	efree(YAF_G(directory));

	YAF_G(directory) = estrndup(directory, len);

	RETURN_ZVAL(self, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Application::getAppDirectory(void)
*/
PHP_METHOD(yaf_application, getAppDirectory) {
	RETURN_STRING(YAF_G(directory), 1);
}
/* }}} */

/** {{{ yaf_application_methods
*/
zend_function_entry yaf_application_methods[] = {
	PHP_ME(yaf_application, __construct, 	yaf_application_construct_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(yaf_application, run, 	 	 	yaf_application_run_arginfo, 		ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, execute,	  	yaf_application_execute_arginfo, 	ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, app, 	 	 	yaf_application_app_arginfo, 		ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	YAF_ME(yaf_application_environ, "environ", yaf_application_environ_arginfo, 	ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, bootstrap,   	yaf_application_bootstrap_arginfo,  ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, getConfig,   	yaf_application_getconfig_arginfo, 	ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, getModules,   	yaf_application_getmodule_arginfo,  ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, getDispatcher, 	yaf_application_getdispatch_arginfo,ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, setAppDirectory,yaf_application_setappdir_arginfo,  ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, getAppDirectory,yaf_application_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, getLastErrorNo, yaf_application_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, getLastErrorMsg,yaf_application_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, clearLastError, yaf_application_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_application, __destruct,		NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
	PHP_ME(yaf_application, __clone,		NULL, ZEND_ACC_PRIVATE|ZEND_ACC_CLONE)
	PHP_ME(yaf_application, __sleep,		NULL, ZEND_ACC_PRIVATE)
	PHP_ME(yaf_application, __wakeup,		NULL, ZEND_ACC_PRIVATE)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
*/
YAF_STARTUP_FUNCTION(application) {
	zend_class_entry ce;
	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Application", "Yaf\\Application", yaf_application_methods);

	yaf_application_ce 			  = zend_register_internal_class_ex(&ce, NULL, NULL TSRMLS_CC);
	yaf_application_ce->ce_flags |= ZEND_ACC_FINAL_CLASS;

	zend_declare_property_null(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_CONFIG), 	ZEND_ACC_PROTECTED 	TSRMLS_CC);
	zend_declare_property_null(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_DISPATCHER), ZEND_ACC_PROTECTED 	TSRMLS_CC);
	zend_declare_property_null(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_APP), 	 	ZEND_ACC_STATIC|ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_MODULES), 	ZEND_ACC_PROTECTED TSRMLS_CC);

	zend_declare_property_bool(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_RUN),	 0, ZEND_ACC_PROTECTED 	TSRMLS_CC);
	zend_declare_property_string(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_ENV), YAF_G(environ), ZEND_ACC_PROTECTED TSRMLS_CC);

	zend_declare_property_long(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_ERRNO), 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(yaf_application_ce, ZEND_STRL(YAF_APPLICATION_PROPERTY_NAME_ERRMSG), "", ZEND_ACC_PROTECTED TSRMLS_CC);

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
