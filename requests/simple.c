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

/* $Id: simple.c 327591 2012-09-10 10:56:13Z laruence $ */

static zend_class_entry *yaf_request_simple_ce;

/** {{{ yaf_request_t * yaf_request_simple_instance(yaf_request_t *this_ptr, zval *module, zval *controller, zval *action, zval *method, zval *params TSRMLS_DC)
*/
yaf_request_t * yaf_request_simple_instance(yaf_request_t *this_ptr, zval *module, zval *controller, zval *action, zval *method, zval *params TSRMLS_DC) {
	yaf_request_t *instance;

	/* 如果传入了已经初始化的类的实例则直接使用，没有的话就初始化一个yaf_request_simple_ce的实例 */
	if (this_ptr) {
		instance = this_ptr;
	} else {
		MAKE_STD_ZVAL(instance);
		object_init_ex(instance, yaf_request_simple_ce);
	}

	if (!method || IS_STRING != Z_TYPE_P(method)) {
		/* 没有传入method或者method类型不为字符串，则初始化method并去sapi中去获取 */
		MAKE_STD_ZVAL(method);
		/** 
     	 *	1.如果能从sapi中获取到request_method的话则直接将取到的request_method的值复制给method,
     	 *	2.如果sapi_module.name的前三个字符是cli的话则method的值为cli
     	 *	3.都不符合上面的情况的话则给method赋值为Unknow
     	 */
		if (!SG(request_info).request_method) {
			if (!strncasecmp(sapi_module.name, "cli", 3)) {
				ZVAL_STRING(method, "CLI", 1);
			} else {
				ZVAL_STRING(method, "Unknow", 1);
			}
		} else {
			ZVAL_STRING(method, (char *)SG(request_info).request_method, 1);
		}
	} else {
		/* method.refcount__gc++ 后面还要使用 */
		Z_ADDREF_P(method);
	}

	/* $this->method = $method */
	zend_update_property(yaf_request_simple_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_METHOD), method TSRMLS_CC);
	zval_ptr_dtor(&method);

	if (module || controller || action) {
		/* module/controller/action任意一个传入*/

		/** 
		 *	如果传入的module存在并且类型为字符串：$this->module = $module 
		 *	相反的，$this->module的值取配置文件中默认的module
		 */
		if (!module || Z_TYPE_P(module) != IS_STRING) {
			zend_update_property_string(yaf_request_simple_ce, instance,
				   	ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE), YAF_G(default_module) TSRMLS_CC);
		} else {
			zend_update_property(yaf_request_simple_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE), module TSRMLS_CC);
		}

		/** 
		 *	如果传入的controller存在并且类型为字符串：$this->controller = $controller 
		 *	相反的，$this->controller的值取配置文件中默认的controller
		 */
		if (!controller || Z_TYPE_P(controller) != IS_STRING) {
			zend_update_property_string(yaf_request_simple_ce, instance,
				   	ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), YAF_G(default_controller) TSRMLS_CC);
		} else {
			zend_update_property(yaf_request_simple_ce, instance,
					ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), controller TSRMLS_CC);
		}

		/** 
		 *	如果传入的action存在并且类型为字符串：$this->action = $action 
		 *	相反的，$this->action的值取配置文件中默认的action
		 */
		if (!action || Z_TYPE_P(action) != IS_STRING) {
			zend_update_property_string(yaf_request_simple_ce, instance,
				   	ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), YAF_G(default_action) TSRMLS_CC);
		} else {
			zend_update_property(yaf_request_simple_ce, instance,
				   	ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), action TSRMLS_CC);
		}
		/* $this->routed = 1 */
		zend_update_property_bool(yaf_request_simple_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ROUTED), 1 TSRMLS_CC);
	} else {
		/* module/controller/action三个都没传入 */

		zval *argv, **ppzval;
		char *query = NULL;
		/* $_SERVER['argv'] 传递给该脚本的参数的数组。当脚本以命令行方式运行时，argv 变量传递给程序 C 语言样式的命令行参数。当通过 GET 方式调用时，该变量包含query string。 */
		argv = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("argv") TSRMLS_CC);
		if (IS_ARRAY == Z_TYPE_P(argv)) {
			/* 对$_SERVER['argv']数组进行遍历 */
			for(zend_hash_internal_pointer_reset(Z_ARRVAL_P(argv));
					zend_hash_has_more_elements(Z_ARRVAL_P(argv)) == SUCCESS;
					zend_hash_move_forward(Z_ARRVAL_P(argv))) {
				if (zend_hash_get_current_data(Z_ARRVAL_P(argv), (void**)&ppzval) == FAILURE) {
					continue;
				} else {
					if (Z_TYPE_PP(ppzval) == IS_STRING) {
						/* 比较当前遍历出的值ppzval的字符串值中是否以字符串request_uri=开始 */
						if (strncasecmp(Z_STRVAL_PP(ppzval), YAF_REQUEST_SERVER_URI, sizeof(YAF_REQUEST_SERVER_URI) - 1)) {
							continue;
						}
						/* 将符合要求的ppzval的字符串截取request_uri=后面的部分赋值给query */
						query = estrdup(Z_STRVAL_PP(ppzval) + sizeof(YAF_REQUEST_SERVER_URI));
						break;
					}
				}
			}
		}

		if (query) {
			/* query有值，$this->uri = $query */
			zend_update_property_string(yaf_request_simple_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_URI), query TSRMLS_CC);
		} else {
			/* query没值，$this->uri = '' */
			zend_update_property_string(yaf_request_simple_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_URI), "" TSRMLS_CC);
		}
		zval_ptr_dtor(&argv);
	}

	/** 
	 *	传入params并且类型为数组，则$this->params = $params 
	 *	没传入params或者类型不为数组,$this->params = array()
	 */
	if (!params || IS_ARRAY != Z_TYPE_P(params)) {
		MAKE_STD_ZVAL(params);
		array_init(params);
		zend_update_property(yaf_request_simple_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), params TSRMLS_CC);
		zval_ptr_dtor(&params);
	} else {
		zend_update_property(yaf_request_simple_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), params TSRMLS_CC);
	}
	/* return $this */
	return instance;
}
/* }}} */

/** {{{ proto public Yaf_Request_Simple::__construct(string $method, string $module, string $controller, string $action, array $params = NULL)
*/
PHP_METHOD(yaf_request_simple, __construct) {
	zval *module 	 = NULL;
	zval *controller = NULL;
	zval *action 	 = NULL;
	zval *params	 = NULL;
	zval *method	 = NULL;
	zval *self 		 = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|zzzzz", &method, &module, &controller, &action, &params) == FAILURE) {
		/*
		 *	zval_dtor(self);
		 *	ZVAL_FALSE(obj);
		 */
		YAF_UNINITIALIZED_OBJECT(getThis());
		return;
	} else {
		if ((params && IS_ARRAY != Z_TYPE_P(params))) {
			/* 如果传入的的$params不是数组的话，则报错 */
			YAF_UNINITIALIZED_OBJECT(getThis());
			yaf_trigger_error(YAF_ERR_TYPE_ERROR TSRMLS_CC,
				   	"Expects the params is an array", yaf_request_simple_ce->name);
			RETURN_FALSE;
		}

		(void)yaf_request_simple_instance(self, module, controller, action, method, params TSRMLS_CC);
	}
}
/* }}} */

/** {{{ proto public Yaf_Request_Simple::getQuery(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_simple, Query, 	YAF_GLOBAL_VARS_GET);
/* }}} */

/** {{{ proto public Yaf_Request_Simple::getPost(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_simple, Post,  	YAF_GLOBAL_VARS_POST);
/* }}} */

/** {{{ proto public Yaf_Request_Simple::getRequet(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_simple, Request, YAF_GLOBAL_VARS_REQUEST);
/* }}} */

/** {{{ proto public Yaf_Request_Simple::getFiles(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_simple, Files, 	YAF_GLOBAL_VARS_FILES);
/* }}} */

/** {{{ proto public Yaf_Request_Simple::getCookie(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_simple, Cookie, 	YAF_GLOBAL_VARS_COOKIE);
/* }}} */

/** {{{ proto public Yaf_Request_Simple::isXmlHttpRequest()
*/
PHP_METHOD(yaf_request_simple, isXmlHttpRequest) {
	/* $_SERVER['X-Requested-With'] */
	zval * header = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("X-Requested-With") TSRMLS_CC);
	/*	if($_SERVER['X-Requested-With'] === 'XMLHttpRequest') {
	 *		return true;	
	 *	} else {
	 *		return false;
	 *	}
	 */
	if (Z_TYPE_P(header) == IS_STRING
			&& strncasecmp("XMLHttpRequest", Z_STRVAL_P(header), Z_STRLEN_P(header)) == 0) {
		zval_ptr_dtor(&header);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&header);
	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_Request_Simple::get(mixed $name, mixed $default)
 * params -> post -> get -> cookie -> server
 */
PHP_METHOD(yaf_request_simple, get) {
	char	*name 	= NULL;
	int 	len	 	= 0;
	zval 	*def 	= NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &name, &len, &def) == FAILURE) {
		WRONG_PARAM_COUNT;
	} else {
		/* 首先尝试从$params中获取，找到的话就直接返回，没有找到的话就进行下面的操作 */
		zval *value = yaf_request_get_param(getThis(), name, len TSRMLS_CC);
		if (value) {
			RETURN_ZVAL(value, 1, 0);
		} else {
			/* 如果在$params中没有找到，那就依次在post->get->cookie->server中进行查找，找到后立马就返回 */
			zval *params	= NULL;
			zval **ppzval	= NULL;

			YAF_GLOBAL_VARS_TYPE methods[4] = {
				YAF_GLOBAL_VARS_POST,
				YAF_GLOBAL_VARS_GET,
				YAF_GLOBAL_VARS_COOKIE,
				YAF_GLOBAL_VARS_SERVER
			};

			{
				int i = 0;
				for (;i<4; i++) {
					params = PG(http_globals)[methods[i]];
					if (params && Z_TYPE_P(params) == IS_ARRAY) {
						if (zend_hash_find(Z_ARRVAL_P(params), name, len + 1, (void **)&ppzval) == SUCCESS ){
							RETURN_ZVAL(*ppzval, 1, 0);
						}
					}
				}

			}
			/* 如果上面的五个都没有找到的话就返回默认值 */
			if (def) {
				RETURN_ZVAL(def, 1, 0);
			}
		}
	}
	/* 如果连默认值都没有的话，只能返回NULL了 */
	RETURN_NULL();
}
/* }}} */

/** {{{ proto private Yaf_Request_Simple::__clone
 */
PHP_METHOD(yaf_request_simple, __clone) {
}
/* }}} */

/** {{{ yaf_request_simple_methods
 */
zend_function_entry yaf_request_simple_methods[] = {
	PHP_ME(yaf_request_simple, __construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(yaf_request_simple, __clone,		NULL, ZEND_ACC_PRIVATE|ZEND_ACC_CLONE)
	PHP_ME(yaf_request_simple, getQuery, 	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_simple, getRequest, 	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_simple, getPost, 		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_simple, getCookie,	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_simple, getFiles,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_simple, get,			NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_simple, isXmlHttpRequest,NULL,ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
 */
YAF_STARTUP_FUNCTION(request_simple){
	zend_class_entry ce;
	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Request_Simple", "Yaf\\Request\\Simple", yaf_request_simple_methods);
	/* final class Yaf_Request_Simple extends Yaf_Request_Abstract */
	yaf_request_simple_ce = zend_register_internal_class_ex(&ce, yaf_request_ce, NULL TSRMLS_CC);
	yaf_request_simple_ce->ce_flags |= ZEND_ACC_FINAL_CLASS;

	/**
	 *	constant SCHEME_HTTP = 'http';
	 *	constant SCHEME_HTTPS = 'https';
	 */
	zend_declare_class_constant_string(yaf_request_simple_ce, ZEND_STRL("SCHEME_HTTP"),  "http" TSRMLS_CC);
	zend_declare_class_constant_string(yaf_request_simple_ce, ZEND_STRL("SCHEME_HTTPS"), "https" TSRMLS_CC);

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
