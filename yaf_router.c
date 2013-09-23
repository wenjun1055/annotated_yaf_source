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

/* $Id: yaf_router.c 327425 2012-09-02 03:58:49Z laruence $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "Zend/zend_alloc.h"
#include "Zend/zend_interfaces.h"
#include "ext/pcre/php_pcre.h"

#include "php_yaf.h"
#include "yaf_namespace.h"
#include "yaf_application.h"
#include "yaf_exception.h"
#include "yaf_request.h"
#include "yaf_router.h"
#include "yaf_config.h"
#include "routes/interface.c"

zend_class_entry *yaf_router_ce;

/** {{{ yaf_router_t * yaf_router_instance(yaf_router_t *this_ptr TSRMLS_DC)
 *	进行一些列初始化，最后返回$this，本类不是单例
 */
yaf_router_t * yaf_router_instance(yaf_router_t *this_ptr TSRMLS_DC) {
	zval 			*routes;
	yaf_router_t 	*instance;
	yaf_route_t		*route;

	if (this_ptr) {
		instance = this_ptr;
	} else {
		/* $instance = new Yaf_Router() */
		MAKE_STD_ZVAL(instance);
		object_init_ex(instance, yaf_router_ce);
	}
	/* $routes = array() */
	MAKE_STD_ZVAL(routes);
	array_init(routes);

	/* 在没有设置默认路由的情况下，默认的栈底总是名为"default"的Yaf_Route_Static路由协议的实例. */
	if (!YAF_G(default_route)) {	/* 没有设置默认路由 */
static_route:
		/* $route = new Yaf_Route_Static() */
	    MAKE_STD_ZVAL(route);
		object_init_ex(route, yaf_route_static_ce);
	} else {
		/* $route = new YAF_G(default_route)() */
		route = yaf_route_instance(NULL, YAF_G(default_route) TSRMLS_CC);
		if (!route) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to initialize default route, use %s instead", yaf_route_static_ce->name);
			goto static_route;
		}
	}
	/* $routes['_default'] = $route*/
	zend_hash_update(Z_ARRVAL_P(routes), "_default", sizeof("_default"), (void **)&route, sizeof(zval *), NULL);
	/* $instance->_routes = $routes */
	zend_update_property(yaf_router_ce, instance, ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_ROUTERS), routes TSRMLS_CC);
	zval_ptr_dtor(&routes);
	/* return $this */
	return instance;
}
/** }}} */

/** {{{ int yaf_router_route(yaf_router_t *router, yaf_request_t *request TSRMLS_DC)
 *	遍历路由器已有的路由协议栈，进行request的解析，执行成功就停止，并且设置$this->_current_route的值为当前解析成功的路由的名称
 */
int yaf_router_route(yaf_router_t *router, yaf_request_t *request TSRMLS_DC) {
	zval 		*routers, *ret;
	yaf_route_t	**route;
	HashTable 	*ht;
	/* $routers = $this->_routes */
	routers = zend_read_property(yaf_router_ce, router, ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_ROUTERS), 1 TSRMLS_CC);

	ht = Z_ARRVAL_P(routers);
	for(zend_hash_internal_pointer_end(ht);
			zend_hash_has_more_elements(ht) == SUCCESS;
			zend_hash_move_backwards(ht)) {

		if (zend_hash_get_current_data(ht, (void**)&route) == FAILURE) {
			continue;
		}
		/* 调用Yaf_Route_Interface::route() */
		zend_call_method_with_1_params(route, Z_OBJCE_PP(route), NULL, "route", &ret, request);

		if (IS_BOOL != Z_TYPE_P(ret) || !Z_BVAL_P(ret)) {	/* 调用执行失败，继续执行 */
			zval_ptr_dtor(&ret);
			continue;
		} else {	/* 调用执行成功 */
			char *key;
			uint len = 0;
			ulong idx = 0;

			switch(zend_hash_get_current_key_ex(ht, &key, &len, &idx, 0, NULL)) {
				case HASH_KEY_IS_LONG:
					/* $this->_current = $this->$idx */
					zend_update_property_long(yaf_router_ce, router, ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_CURRENT_ROUTE), idx TSRMLS_CC);
					break;
				case HASH_KEY_IS_STRING:
					if (len) {
						/* $this->_current = $this->$idx */
						zend_update_property_string(yaf_router_ce, router, ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_CURRENT_ROUTE), key TSRMLS_CC);
					}
					break;
			}
			/* Yaf_Request_Abstract::routed = 1 */
			yaf_request_set_routed(request, 1 TSRMLS_CC);
			zval_ptr_dtor(&ret);
			break;
		}
	}
	return 1;
}
/* }}} */

/** {{{ int yaf_router_add_config(yaf_router_t *router, zval *configs TSRMLS_DC)
 *	遍历配置项，初始化相应的继承自Yaf_Route_Interface的路由类，并且放入路由器的路由数组中
 */
int yaf_router_add_config(yaf_router_t *router, zval *configs TSRMLS_DC) {
	zval 		**entry;
	HashTable 	*ht;
	yaf_route_t *route;

	if (!configs || IS_ARRAY != Z_TYPE_P(configs)) {
		return 0;
	} else {
		char *key = NULL;
		uint len = 0;
		ulong idx = 0;
		zval *routes;
		/* $routes = $this->_routes */
		routes = zend_read_property(yaf_router_ce, router, ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_ROUTERS), 1 TSRMLS_CC);

		ht = Z_ARRVAL_P(configs);
		for(zend_hash_internal_pointer_reset(ht);
				zend_hash_has_more_elements(ht) == SUCCESS;
				zend_hash_move_forward(ht)) {
			if (zend_hash_get_current_data(ht, (void**)&entry) == FAILURE) {
				continue;
			}

			if (!entry || Z_TYPE_PP(entry) != IS_ARRAY) {
				continue;
			}
			/* 通过配置相应的实例化Yaf_Route_Interface的继承类 */
			route = yaf_route_instance(NULL, *entry TSRMLS_CC);
			switch (zend_hash_get_current_key_ex(ht, &key, &len, &idx, 0, NULL)) {
				case HASH_KEY_IS_STRING:
					if (!route) {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to initialize route named '%s'", key);
						continue;
					}
					/* $routes[$key] = $route */
					zend_hash_update(Z_ARRVAL_P(routes), key, len, (void **)&route, sizeof(zval *), NULL);
					break;
				case HASH_KEY_IS_LONG:
					if (!route) {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to initialize route at index '%ld'", idx);
						continue;
					}
					/* $routes[] = $route */
					zend_hash_index_update(Z_ARRVAL_P(routes), idx, (void **)&route, sizeof(zval *), NULL);
					break;
				default:
					continue;
			}
		}
		return 1;
	}
}
/* }}} */

/** {{{ zval * yaf_router_parse_parameters(char *uri TSRMLS_DC)
 *	在本类中没调用，但是yaf_route_map.c、yaf_route_rewrite.c、yaf_route_static.c会调用
 *	分割uri得到传递进来的参数的键值对
 */
zval * yaf_router_parse_parameters(char *uri TSRMLS_DC) {
	char *key, *ptrptr, *tmp, *value;
	zval *params, *val;
	uint key_len;
	/* $params = array() */
	MAKE_STD_ZVAL(params);
	array_init(params);

	tmp = estrdup(uri);
	/* #define YAF_ROUTER_URL_DELIMIETER    "/" */
	/**  
	 *	$uri = 'key1/value1/key2/value2/key3';
	 *	params = array(
	 *		'key1' => 'value1'
	 *		'key2' => 'value2'
	 *		'key3' => null
	 *	)
	 */
	key = php_strtok_r(tmp, YAF_ROUTER_URL_DELIMIETER, &ptrptr);
	while (key) {
		key_len = strlen(key);
		if (key_len) {
			MAKE_STD_ZVAL(val);
			value = php_strtok_r(NULL, YAF_ROUTER_URL_DELIMIETER, &ptrptr);
			if (value && strlen(value)) {
				/* $val = $value */
				ZVAL_STRING(val, value, 1);
			} else {
				/* $val = null */
				ZVAL_NULL(val);
			}
			/* $params[$key] = $val */
			zend_hash_update(Z_ARRVAL_P(params), key, key_len + 1, (void **)&val, sizeof(zval *), NULL);
		}

		key = php_strtok_r(NULL, YAF_ROUTER_URL_DELIMIETER, &ptrptr);
	}

	efree(tmp);

	return params;
}
/* }}} */

/** {{{ proto public Yaf_Router::__construct(void)
 */
PHP_METHOD(yaf_router, __construct) {
	yaf_router_instance(getThis() TSRMLS_CC);
}
/* }}} */

/** {{{ proto public Yaf_Router::route(Yaf_Request $req)
 *	路由一个请求, 本方法不需要主动调用, Yaf_Dispatcher::dispatch会自动调用本方法
 */
PHP_METHOD(yaf_router, route) {
	yaf_request_t *request;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &request) == FAILURE) {
		return;
	} else {
		/* 遍历路由器已有的路由协议栈，进行request的解析，执行成功就停止，并且设置$this->_current_route的值为当前解析成功的路由的名称 */
		RETURN_BOOL(yaf_router_route(getThis(), request TSRMLS_CC));
	}
}
/* }}} */

/** {{{  proto public Yaf_Router::addRoute(string $name, Yaf_Route_Interface $route)
 *	给路由器增加一个名为$name的路由协议。成功返回Yaf_Router, 失败返回FALSE, 并抛出异常(或者触发错误)
 */
PHP_METHOD(yaf_router, addRoute) {
	char 	   *name;
	zval 	   *routes;
	yaf_route_t *route;
	uint	   len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name, &len, &route) == FAILURE) {
		return;
	}

	if (!len) {
		RETURN_FALSE;
	}
	/* route必须是Yaf_Route_Interface的一个实例 */
	if (IS_OBJECT != Z_TYPE_P(route)
			|| !instanceof_function(Z_OBJCE_P(route), yaf_route_ce TSRMLS_CC)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expects a %s instance", yaf_route_ce->name);
		RETURN_FALSE;
	}
	/* $routes = $this->_routes */
	routes = zend_read_property(yaf_router_ce, getThis(), ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_ROUTERS), 1 TSRMLS_CC);
	/* route.refcount__gc++ */
	Z_ADDREF_P(route);
	/* $routes[$name] = $route */
	zend_hash_update(Z_ARRVAL_P(routes), name, len + 1, (void **)&route, sizeof(zval *), NULL);
	/* return $this */
	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{  proto public Yaf_Router::addConfig(Yaf_Config_Abstract $config)
 *	给路由器通过配置增加一簇路由协议。成功返回$this, 失败返回FALSE, 并抛出异常(或者触发错误)	
 */
PHP_METHOD(yaf_router, addConfig) {
	yaf_config_t *config;
	zval		 *routes;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &config) == FAILURE) {
		return;
	}
	
	if (IS_OBJECT == Z_TYPE_P(config) && instanceof_function(Z_OBJCE_P(config), yaf_config_ce TSRMLS_CC)){	/* $config为一个Yaf_Config_Abstract的实例 */
		/* $routes = Yaf_Config_Abstract::$_config */
		routes = zend_read_property(yaf_config_ce, config, ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
	} else if (IS_ARRAY == Z_TYPE_P(config)) {	/* $config为数组 */
		/* $routes = $config */
		routes = config;
	} else {	/* 两个都不是，报错 */
		php_error_docref(NULL TSRMLS_CC, E_WARNING,  "Expect a %s instance or an array, %s given", yaf_config_ce->name, zend_zval_type_name(config));
		RETURN_FALSE;
	}
	/* 根据配置，进行路由的配置以及初始化 */
	if (yaf_router_add_config(getThis(), routes TSRMLS_CC)) {
		RETURN_ZVAL(getThis(), 1, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/** {{{  proto public Yaf_Router::getRoute(string $name)
 *	获取当前路由器的路由协议栈中名为$name的协议
 */
PHP_METHOD(yaf_router, getRoute) {
	char  *name;
	uint  len;
	zval  *routes;
	yaf_route_t **route;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &len) == FAILURE) {
		return;
	}

	if (!len) {
		RETURN_FALSE;
	}
	/* $routes = $this->_routes */
	routes = zend_read_property(yaf_router_ce, getThis(), ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_ROUTERS), 1 TSRMLS_CC);
	/**  
	 *	if (isset($routes[$name])) {
	 *		return $routes[$name];
	 *	} else {
	 *  	return null;
	 *	}
	 */
	if (zend_hash_find(Z_ARRVAL_P(routes), name, len + 1, (void **)&route) == SUCCESS) {
		RETURN_ZVAL(*route, 1, 0);
	}

	RETURN_NULL();
}
/* }}} */

/** {{{  proto public Yaf_Router::getRoutes(void)
 *	获取当前路由器中的所有路由协议。成功返回当前路由器的路由协议栈内容, 失败返回FALSE
 */
PHP_METHOD(yaf_router, getRoutes) {
	/* $routes = $this->_routes */
	zval * routes = zend_read_property(yaf_router_ce, getThis(), ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_ROUTERS), 1 TSRMLS_CC);
	/* return $routes */
	RETURN_ZVAL(routes, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Router::isModuleName(string $name)
 *	判断一个Module名, 是否是申明存在的Module。如果是返回TRUE, 不是返回FALSE
 */
PHP_METHOD(yaf_router, isModuleName) {
	char *name;
	uint len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &len) == FAILURE) {
		return;
	}
	/* 判断名称是否是已经注册了的module的名称 */
	RETURN_BOOL(yaf_application_is_module_name(name, len TSRMLS_CC));
}
/* }}} */

/** {{{  proto public Yaf_Router::getCurrentRoute(void)
 *	在路由结束以后, 获取路由匹配成功, 路由生效的路由协议名。成功返回生效的路由协议名, 失败返回NULL
 */
PHP_METHOD(yaf_router, getCurrentRoute) {
	/* $route = $this->_current; */
	zval *route = zend_read_property(yaf_router_ce, getThis(), ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_CURRENT_ROUTE), 1 TSRMLS_CC);
	/* return $route */
	RETURN_ZVAL(route, 1, 0);
}
/* }}} */

/** {{{ yaf_router_methods
 */
zend_function_entry yaf_router_methods[] = {
	PHP_ME(yaf_router, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(yaf_router, addRoute,  NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_router, addConfig, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_router, route,	 NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_router, getRoute,  NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_router, getRoutes, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_router, getCurrentRoute, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
 */
YAF_STARTUP_FUNCTION(router) {
	zend_class_entry ce;

	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Router", "Yaf\\Router", yaf_router_methods);
	yaf_router_ce = zend_register_internal_class_ex(&ce, NULL, NULL TSRMLS_CC);
	/* final class Yaf_Router */
	yaf_router_ce->ce_flags |= ZEND_ACC_FINAL_CLASS;
	/**
	 *	protected $_routes = null
	 *	protected $_current = null
	 */
	zend_declare_property_null(yaf_router_ce, ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_ROUTERS), 		 ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_router_ce, ZEND_STRL(YAF_ROUTER_PROPERTY_NAME_CURRENT_ROUTE), ZEND_ACC_PROTECTED TSRMLS_CC);

	YAF_STARTUP(route);

	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
