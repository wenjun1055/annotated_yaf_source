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

/* $Id: static.c 327627 2012-09-13 06:12:38Z laruence $ */

zend_class_entry * yaf_route_static_ce;

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(yaf_route_static_match_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, uri)
ZEND_END_ARG_INFO()
/* }}} */

/**
 *	分割request_uri，然后经过判断得到module/controller/action/params并赋值给Yaf_Request_Abstract相应的成员变量
 */
static int yaf_route_pathinfo_route(yaf_request_t *request, char *req_uri, int req_uri_len TSRMLS_DC) /* {{{ */ {
	zval *params;
	char *module = NULL, *controller = NULL, *action = NULL, *rest = NULL;
	/* 进行一些列的/分割，从req_uri中得到module/controller/action */
	do {
#define strip_slashs(p) while (*p == ' ' || *p == '/') { ++p; }
		char *s, *p;
		char *uri;
		/* req_uri不存在或者为/ */
		if (req_uri_len == 0
				|| (req_uri_len == 1 && *req_uri == '/')) {
			break;
		}

		uri = req_uri;
		s = p = uri;

		if (req_uri_len) {
			char *q = req_uri + req_uri_len - 1;
			while (q > req_uri && (*q == ' ' || *q == '/')) {
				*q-- = '\0';
			}
		}

		strip_slashs(p);	/* 跳过/或者空格 */


		if ((s = strstr(p, "/")) != NULL) {	/* 第一次截取，尝试获取module名字 */
			if (yaf_application_is_module_name(p, s-p TSRMLS_CC)) {		//是moduel名
				module = estrndup(p, s - p);
				p  = s + 1;
		        strip_slashs(p);	/* 跳过/或者空格 */
				if ((s = strstr(p, "/")) != NULL) {	//再次分割查找controller
					controller = estrndup(p, s - p);
					p  = s + 1;
				}
			} else {	//不是moduel名,把它当做controller
				controller = estrndup(p, s - p);
				p  = s + 1;
			}
		}

		strip_slashs(p);	/* 跳过/或者空格 */
		if ((s = strstr(p, "/")) != NULL) {	/* 还能截取就把截取得到的当做action */
			action = estrndup(p, s - p);
			p  = s + 1;
		}

		strip_slashs(p);	/* 跳过/或者空格 */
		if (*p != '\0') {	/* 经过前面的分割并且跳过过后还没到字符串的结尾 */
			do {
				if (!module && !controller && !action) {	//根本没有切割过
					if (yaf_application_is_module_name(p, strlen(p) TSRMLS_CC)) {	/* 判断整个p是不是module名 */
						module = estrdup(p);
						break;
					}
				}

				if (!controller) {	/* 经过相应分割过后没有得到controller，而字符串还没到结尾，就当剩下的为controller */
					controller = estrdup(p);
					break;
				}

				if (!action) {	/* 经过相应分割过后没有得到action，而字符串还没到结尾，就当剩下的为action */
					action = estrdup(p);
					break;
				}

				rest = estrdup(p);	/* 经过相应分割过后，req_uri还有剩余就放到rest */
			} while (0);
		}

		if (module && controller == NULL) {	/* module/controller/action三个中只成功得到module，而没有得到controller，则认为这个module为controller */
			controller = module;
			module = NULL;
		} else if (module && action == NULL) {	/* module/controller/action三个中只成功得到module，而没有得到action，则认为这个module为controller，controller为action */
			action = controller;
			controller = module;
			module = NULL;
	    } else if (controller && action == NULL ) {	/* module/controller/action三个中只成功得到controller，而没有得到action，到底作为controller还是action根据配置来 */
			/* /controller */
			if (YAF_G(action_prefer)) {	//当PATH_INFO只有一部分的时候，将其作为controller还是action，配置打开则为action
				action = controller;
				controller = NULL;
			}
		}
	} while (0);

	if (module != NULL) {
		/* Yaf_Request_Abstract::$module = $module */
		zend_update_property_string(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE), module TSRMLS_CC);
		efree(module);
	}
	if (controller != NULL) {
		/* Yaf_Request_Abstract::$controller = $controller */
		zend_update_property_string(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), controller TSRMLS_CC);
		efree(controller);
	}

	if (action != NULL) {
		/* Yaf_Request_Abstract::$action = $action */
		zend_update_property_string(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), action TSRMLS_CC);
		efree(action);
	}

	if (rest) {	/* 将request_uri分割module/controller/action后剩余的部分进行参数的键值对的分割 */
		params = yaf_router_parse_parameters(rest TSRMLS_CC);
		/* 将得到的参数的键值对添加到Yaf_Request_Abstract::$params中 */
		(void)yaf_request_set_params_multi(request, params TSRMLS_CC);
		zval_ptr_dtor(&params);
		efree(rest);
	}

	return 1;
}
/* }}} */

/** {{{ int yaf_route_static_route(yaf_route_t *route, yaf_request_t *request TSRMLS_DC)
 *	判断得到request_uri,然后传递给yaf_route_pathinfo_route分割得到module/controller/action/params
 */
int yaf_route_static_route(yaf_route_t *route, yaf_request_t *request TSRMLS_DC) {
	zval *zuri, *base_uri;
	char *req_uri;
	int  req_uri_len;
	/* $zuri = Yaf_Request_Abstract::$uri */
	zuri 	 = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_URI), 1 TSRMLS_CC);
	/* $base_uri = Yaf_Request_Abstract::$_base_uri */
	base_uri = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_BASE), 1 TSRMLS_CC);

	/* 判断然后截取获得真正的request_uri */
	if (base_uri && IS_STRING == Z_TYPE_P(base_uri)
			&& !strncasecmp(Z_STRVAL_P(zuri), Z_STRVAL_P(base_uri), Z_STRLEN_P(base_uri))) {	/* $base_uri为合法字符串，并且$base_uri为$zuri开头的一部分 */
		/* $req_uri = substr($zuri, strlen($base_uri)); */
		req_uri  = estrdup(Z_STRVAL_P(zuri) + Z_STRLEN_P(base_uri));
		/* $req_uri_len = strlen($zuri) - strlen($base_uri) */
		req_uri_len = Z_STRLEN_P(zuri) - Z_STRLEN_P(base_uri);
	} else {
		/* $req_uri = $zuri; */
		req_uri  = estrdup(Z_STRVAL_P(zuri));
		/* $req_uri_len = strlen($zuri); */
		req_uri_len = Z_STRLEN_P(zuri);
	}

	yaf_route_pathinfo_route(request, req_uri, req_uri_len TSRMLS_CC);
	efree(req_uri);
	return 1;
}
/* }}} */

/** {{{ proto public Yaf_Router_Classical::route(Yaf_Request $req)
*/
PHP_METHOD(yaf_route_static, route) {
	yaf_request_t *request;
	/* 接收Yaf_Request_Abstract的实例 */
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, yaf_request_ce) == FAILURE) {
		return;
	} else {
		RETURN_BOOL(yaf_route_static_route(getThis(), request TSRMLS_CC));
	}
}
/* }}} */

/** {{{ proto public Yaf_Router_Classical::match(string $uri)
*/
PHP_METHOD(yaf_route_static, match) {
	RETURN_TRUE;
}
/* }}} */

/** {{{ yaf_route_static_methods
 */
zend_function_entry yaf_route_static_methods[] = {
	PHP_ME(yaf_route_static, match, yaf_route_static_match_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_route_static, route, yaf_route_route_arginfo, 		ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
 */
YAF_STARTUP_FUNCTION(route_static) {
	zend_class_entry ce;

	/* final class Yaf_Route_Static implements Yaf_Route_Interface */
	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Route_Static", "Yaf\\Route_Static", yaf_route_static_methods);
	yaf_route_static_ce = zend_register_internal_class_ex(&ce, NULL, NULL TSRMLS_CC);
	zend_class_implements(yaf_route_static_ce TSRMLS_CC, 1, yaf_route_ce);

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

