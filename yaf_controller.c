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

/* $Id: yaf_controller.c 328819 2012-12-18 08:06:57Z remi $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "main/SAPI.h"
#include "Zend/zend_API.h"
#include "Zend/zend_interfaces.h"

#include "php_yaf.h"
#include "yaf_namespace.h"
#include "yaf_request.h"
#include "yaf_response.h"
#include "yaf_dispatcher.h"
#include "yaf_view.h"
#include "yaf_exception.h"
#include "yaf_action.h"
#include "yaf_controller.h"

zend_class_entry * yaf_controller_ce;

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(yaf_controller_void_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_controller_initview_arginfo, 0, 0, 0)
    ZEND_ARG_ARRAY_INFO(0, options, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_controller_getiarg_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_controller_setvdir_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, view_directory)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_controller_forward_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, module)
    ZEND_ARG_INFO(0, controller)
    ZEND_ARG_INFO(0, action)
    ZEND_ARG_ARRAY_INFO(0, paramters, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_controller_redirect_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_controller_render_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, tpl)
    ZEND_ARG_ARRAY_INFO(0, parameters, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_controller_display_arginfo, 0, 0, 1)
    ZEND_ARG_INFO(0, tpl)
    ZEND_ARG_ARRAY_INFO(0, parameters, 1)
ZEND_END_ARG_INFO()
/* }}} */

/** {{{ zval * yaf_controller_render(yaf_controller_t *instance, char *action_name, int len, zval *var_array TSRMLS_DC)
 */
zval * yaf_controller_render(yaf_controller_t *instance, char *action_name, int len, zval *var_array TSRMLS_DC) {
	char 	*path, *view_ext, *self_name, *tmp;
	zval 	*name, *param, *ret = NULL;
	int 	path_len;
	yaf_view_t *view;
	zend_class_entry *view_ce;
	/* 获取$this->_view */
	view   	  = zend_read_property(yaf_controller_ce, instance, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_VIEW), 0 TSRMLS_CC);
	/* 获取$this->_name */
	name	  = zend_read_property(yaf_controller_ce, instance, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_NAME), 0 TSRMLS_CC);
	/* 模板文件的后缀 */
	view_ext  = YAF_G(view_ext);	
	/* 将$this->_name转为小写 */
	self_name = zend_str_tolower_dup(Z_STRVAL_P(name), Z_STRLEN_P(name));

	tmp = self_name;
 	while (*tmp != '\0') {
		if (*tmp == '_') {
			*tmp = DEFAULT_SLASH;
		}
		tmp++;
	}
	/* 复制action_name */
	action_name = estrndup(action_name, len);

	tmp = action_name;
 	while (*tmp != '\0') {
		if (*tmp == '_') {
			*tmp = DEFAULT_SLASH;
		}
		tmp++;
	}
	/* $path = "index/index.phtml" */
	path_len  = spprintf(&path, 0, "%s%c%s.%s", self_name, DEFAULT_SLASH, action_name, view_ext);

	efree(self_name);
	efree(action_name);
	/* $param = $path; */
	MAKE_STD_ZVAL(param);
	ZVAL_STRINGL(param, path, path_len, 0);
	/* 调用Yaf_View_*的成员方法render进行渲染,并将渲染后的结果赋值给ret返回 */
	view_ce = Z_OBJCE_P(view);
	if (var_array) {
		zend_call_method_with_2_params(&view, view_ce, NULL, "render", &ret, param, var_array);
	} else {
		zend_call_method_with_1_params(&view, view_ce, NULL, "render", &ret, param);
	}
	
	zval_ptr_dtor(&param);

	if (!ret) {
		return NULL;
	}
	
	if (EG(exception)) {
		zval_ptr_dtor(&ret);
		return NULL;
	}

	if (Z_TYPE_P(ret) == IS_BOOL && !Z_BVAL_P(ret)) {
		zval_ptr_dtor(&ret);
		return NULL;
	}
	/* 结果不为false,返回渲染后的内容 */
	return ret;
}
/* }}} */

/** {{{ int yaf_controller_display(yaf_controller_t *instance, char *action_name, int len, zval *var_array TSRMLS_DC)
 */
int yaf_controller_display(yaf_controller_t *instance, char *action_name, int len, zval *var_array TSRMLS_DC) {
	char *path, *view_ext, *self_name, *tmp;
	zval *name, *param, *ret = NULL;
	int  path_len;
	yaf_view_t	*view;
	/* 获取$this->_view */
	view   	  = zend_read_property(yaf_controller_ce, instance, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_VIEW), 1 TSRMLS_CC);
	/* 获取$this->_name */
	name	  = zend_read_property(yaf_controller_ce, instance, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_NAME), 1 TSRMLS_CC);
	/* 模板文件的后缀 */
	view_ext  = YAF_G(view_ext);
	/* 将$this->_name转为小写 */
	self_name = zend_str_tolower_dup(Z_STRVAL_P(name), Z_STRLEN_P(name));

	tmp = self_name;
	while (*tmp != '\0') {
		if (*tmp == '_') {
			*tmp = DEFAULT_SLASH;
		}
		tmp++;
	}
	/* 复制action_name */
	action_name = estrndup(action_name, len);

	tmp = action_name;
 	while (*tmp != '\0') {
		if (*tmp == '_') {
			*tmp = DEFAULT_SLASH;
		}
		tmp++;
	}
	/* $path = "index/index.phtml" */
	path_len  = spprintf(&path, 0, "%s%c%s.%s", self_name, DEFAULT_SLASH, action_name, view_ext);

	efree(self_name);
	efree(action_name);
	/* $param = $path; */
	MAKE_STD_ZVAL(param);
	ZVAL_STRINGL(param, path, path_len, 0);
	/* 调用Yaf_View_*的成员方法display进行渲染并输出给用户 */
	if (var_array) {
		zend_call_method_with_2_params(&view, Z_OBJCE_P(view), NULL, "display", &ret, param, var_array);
	} else {
		zend_call_method_with_1_params(&view, Z_OBJCE_P(view), NULL, "display", &ret, param);
	}
	zval_ptr_dtor(&param);

	if (!ret) {
		return 0;
	}

	if (EG(exception)) {
		zval_ptr_dtor(&ret);
		return 0;
	}

	if ((Z_TYPE_P(ret) == IS_BOOL && !Z_BVAL_P(ret))) {
		zval_ptr_dtor(&ret);
		return 0;
	}

	zval_ptr_dtor(&ret);

	return 1;
}
/* }}} */

/** {{{ int yaf_controller_construct(zend_class_entry *ce, yaf_controller_t *self, yaf_request_t *request, yaf_response_t *responseew_t *view, zval *args TSRMLS_DC)
 *	Yaf_Controller_Abstract::__construct
 */
int yaf_controller_construct(zend_class_entry *ce, yaf_controller_t *self, yaf_request_t *request, yaf_response_t *response, yaf_view_t *view, zval *args TSRMLS_DC) {
	zval *module;

	if (args) {
		/**
		 *	isset($args) && $this->_invoke_args = $args
		 */ 
		zend_update_property(ce, self, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_ARGS), args TSRMLS_CC);
	}
	/* 从Yaf_Request_*类的实例中读取成员变量$module */
	module = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE), 1 TSRMLS_CC);

	/**
	 *	$this->_request = $request;($request为Yaf_Request_*类的实例)
	 *	$this->_response = $response;($request为Yaf_Response_*类的实例)
	 *	$this->_module = $module;($module为Yaf_Request_*的成员变量)
	 *	$this->_view = $view;($view为Yaf_View_Simple的实例)
	 */
	zend_update_property(ce, self, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_REQUEST), request TSRMLS_CC);
	zend_update_property(ce, self, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_RESPONSE), response TSRMLS_CC);
	zend_update_property(ce, self, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_MODULE), module TSRMLS_CC);
	zend_update_property(ce, self, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_VIEW), view TSRMLS_CC);
	/* ce不是Yaf_Action的实例，并且ce的成员函数里面有"init"的话就调用它自身的成员函数init */
	if (!instanceof_function(ce, yaf_action_ce TSRMLS_CC)
			&& zend_hash_exists(&(ce->function_table), ZEND_STRS("init"))) {
		zend_call_method_with_0_params(&self, ce, NULL, "init", NULL);
	}

	return 1;
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::init()
*/
PHP_METHOD(yaf_controller, init) {
}

/* }}} */

/** {{{ proto protected Yaf_Controller_Abstract::__construct(Yaf_Request_Abstract $request, Yaf_Response_abstrct $response, Yaf_View_Interface $view, array $invokeArgs = NULL)
*/
PHP_METHOD(yaf_controller, __construct) {
	yaf_request_t 	*request;
	yaf_response_t	*response;
	yaf_view_t		*view;
	zval 			*invoke_arg = NULL;
	yaf_controller_t *self = getThis();
	/* 三个参数必传 */
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ooo|z",
				&request, yaf_request_ce, &response, yaf_response_ce, &view, yaf_view_interface_ce, &invoke_arg) == FAILURE) {
		return;
	} else	{
		/* 进行类的成员变量的初始化工作，然后调用init方法，失败就返回false */
		if(!yaf_controller_construct(yaf_controller_ce, self, request, response, view, invoke_arg TSRMLS_CC)) {
			RETURN_FALSE;
		}
	}
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::getView(void)
 *	获取视图引擎类的实例
 */
PHP_METHOD(yaf_controller, getView) {
	yaf_view_t *view = zend_read_property(yaf_controller_ce, getThis(), ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_VIEW), 1 TSRMLS_CC);
	RETURN_ZVAL(view, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::getRequest(void)
 *	获取Yaf_Request_*的实例
 */
PHP_METHOD(yaf_controller, getRequest) {
	yaf_request_t *request = zend_read_property(yaf_controller_ce, getThis(), ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_REQUEST), 1 TSRMLS_CC);
	RETURN_ZVAL(request, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::getResponse(void)
 *	获取Yaf_Response_*的实例
 */
PHP_METHOD(yaf_controller, getResponse) {
	yaf_view_t *response = zend_read_property(yaf_controller_ce, getThis(), ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_RESPONSE), 1 TSRMLS_CC);
	RETURN_ZVAL(response, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::initView(array $options = NULL)
 *	TODO 难道是bug
*/
PHP_METHOD(yaf_controller, initView) {
	RETURN_ZVAL(getThis(), 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::getInvokeArg(string $name)
 */
PHP_METHOD(yaf_controller, getInvokeArg) {
	char *name;
	uint len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",  &name, &len) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (len) {
		zval **ppzval, *args;
		/* $args = $this->_invoke_args */
		args = zend_read_property(yaf_controller_ce, getThis(), ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_ARGS), 1 TSRMLS_CC);
		/* if (null === $args) { return null; } */
		if (ZVAL_IS_NULL(args)) {
			RETURN_NULL();
		}
		/* 在$this->_invoke_args成员变量数组中查找name的值，找到就返回，没找到返回NULL */
		if (zend_hash_find(Z_ARRVAL_P(args), name, len + 1, (void **)&ppzval) == SUCCESS) {
			RETURN_ZVAL(*ppzval, 1, 0);
		}
	}
	RETURN_NULL();
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::getInvokeArgs(void)
 *	获取 $this->_invoke_args
 */
PHP_METHOD(yaf_controller, getInvokeArgs) {
	zval *args = zend_read_property(yaf_controller_ce, getThis(), ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_ARGS), 1 TSRMLS_CC);
	RETURN_ZVAL(args, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::getModuleName(void)
 *	获取 $this->_module
 */
PHP_METHOD(yaf_controller, getModuleName) {
	zval *module = zend_read_property(yaf_controller_ce, getThis(), ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_MODULE), 1 TSRMLS_CC);
	RETURN_ZVAL(module, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::setViewpath(string $view_directory)
 *	设置模板文件夹路径
 */
PHP_METHOD(yaf_controller, setViewpath) {
	zval 		*path;
	yaf_view_t 	*view;
	zend_class_entry *view_ce;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &path) == FAILURE) {
		return;
	}
	/* if (is_string($path)) {return false;} */
	if (Z_TYPE_P(path) != IS_STRING) {
		RETURN_FALSE;
	}
	/* $view = $this->_view */
	view = zend_read_property(yaf_controller_ce, getThis(), ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_VIEW), 0 TSRMLS_CC);
	/** 	
	 *	如果view是由Yaf_View_Simple实例化生成的则直接将模板路径赋值给Yaf_View_Simple的成员变量_tpl_dir；
	 *	不然的话则调用Yaf_View_Interface的成员方法setScriptpath来设置path
	 */
	if ((view_ce = Z_OBJCE_P(view)) == yaf_view_simple_ce) {
		zend_update_property(view_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLDIR), path TSRMLS_CC);
	} else {
		zend_call_method_with_1_params(&view, view_ce, NULL, "setscriptpath", NULL, path);
	}
	/* return true; */
	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::getViewpath(void)
 *	获取模板文件夹路径
 */
PHP_METHOD(yaf_controller, getViewpath) {
	zend_class_entry *view_ce;
	/* $view = $this->_view */
	zval *view = zend_read_property(yaf_controller_ce, getThis(), ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_VIEW), 1 TSRMLS_CC);
	/* 如果$view是由Yaf_View_Simple实例化产生的那么就直接读取它的成员变量$_tpl_dir，否则就调用继承了Yaf_View_Interface的类的getScriptpath方法 */
	if ((view_ce = Z_OBJCE_P(view)) == yaf_view_simple_ce) {
		zval *tpl_dir = zend_read_property(view_ce, view, ZEND_STRL(YAF_VIEW_PROPERTY_NAME_TPLDIR), 1 TSRMLS_CC);
		/* 如果Yaf_View_Simple::$_tpl_dir的值不是字符串并且设置了YAF_G(view_directory)就返回YAF_G(view_directory)，否则的话返回Yaf_View_Simple::$_tpl_dir */
		if (IS_STRING != Z_TYPE_P(tpl_dir) && YAF_G(view_directory)) {
			RETURN_STRING(YAF_G(view_directory), 1);
		}
		RETURN_ZVAL(tpl_dir, 1, 0);
	} else {
		zval *ret;
		zend_call_method_with_0_params(&view, view_ce, NULL, "getscriptpath", &ret);
		RETURN_ZVAL(ret, 1, 1);
	}
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::forward($module, $controller, $action, $args = NULL)
*/
PHP_METHOD(yaf_controller, forward) {
	yaf_request_t *request;
	zval *controller, *module, *action, *args, *parameters;
	zend_class_entry *request_ce;

	yaf_controller_t *self = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|zzz", &module, &controller, &action, &args) == FAILURE) {
		return;
	}
	/* 获取Yaf_Request_*的实例$this->_request */
	request    = zend_read_property(yaf_controller_ce, self, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_REQUEST), 1 TSRMLS_CC);
	/* 获取$this->_invoke_args */
	parameters = zend_read_property(yaf_controller_ce, self, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_ARGS), 1 TSRMLS_CC);
	/* 判断$this->_request不是对象或者不是继承自Yaf_Request_Abstract则返回false */
	if (Z_TYPE_P(request) != IS_OBJECT
			|| !instanceof_function((request_ce = Z_OBJCE_P(request)), yaf_request_ce TSRMLS_CC)) {
		RETURN_FALSE;
	}

	/* 如果$this->_invoke_args = null，则给它设置一个数组作为值 */
	if (ZVAL_IS_NULL(parameters)) {
		MAKE_STD_ZVAL(parameters);
		array_init(parameters);
		zend_update_property(yaf_controller_ce, self, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_ARGS), parameters TSRMLS_CC);
		zval_ptr_dtor(&parameters);
	}
	/* 传递的参数可以是1、2、3、4个 */
	switch (ZEND_NUM_ARGS()) {
		case 1:
			/* 1个参数则是action，action名必须为字符串，Yaf_Request_*::$action = $module */
			/* Yaf_Controller_Abstract::forward(string $action); */
			if (Z_TYPE_P(module) != IS_STRING) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expect a string action name");
				RETURN_FALSE;
			}
			zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), module TSRMLS_CC);
			break;
		case 2:
			/* 2个参数 */
			if (Z_TYPE_P(controller) ==  IS_STRING) {
				/**
				 *	第二个参数为字符串：
				 *		Yaf_Request_*::$controller = $mocdule
				 *		Yaf_Request_*::$action = $$controller
				 *	Yaf_Controller_Abstract::forward(string $controller, string $action);
				 */
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), module TSRMLS_CC);
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), controller TSRMLS_CC);
			} else if (Z_TYPE_P(controller) == IS_ARRAY) {
				/**
				 *	第二个参数为数组：
				 *		将action数组的值复制给Yaf_Request_*::$params
				 *		Yaf_Request_*::$action = $mocdule
				 *	Yaf_Controller_Abstract::forward(string $action ,array $params);
				 */
				zend_hash_copy(Z_ARRVAL_P(parameters), Z_ARRVAL_P(controller), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), module TSRMLS_CC);
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), parameters TSRMLS_CC);
			} else {
				RETURN_FALSE;
			}
			break;
		case 3:
			/* 3个参数 */
			if (Z_TYPE_P(action) == IS_STRING) {
				/**
				 *	第三个参数为字符串，则认为它是action名称
				 *		Yaf_Request_*::$module = $module
				 *		Yaf_Request_*::$controller = $controller
				 *		Yaf_Request_*::$action = $action
				 */
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE), module TSRMLS_CC);
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), controller TSRMLS_CC);
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), action TSRMLS_CC);
			} else if (Z_TYPE_P(action) == IS_ARRAY) {
				/**
				 *	第三个参数为数组，则认为它是parameters
				 *		Yaf_Request_*::$controller = $module
				 *		Yaf_Request_*::$action = $controller
				 *		Yaf_Request_*::$params = $parameters
				 */
				zend_hash_copy(Z_ARRVAL_P(parameters), Z_ARRVAL_P(action), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), module TSRMLS_CC);
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), controller TSRMLS_CC);
				zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), parameters TSRMLS_CC);
			} else {
				RETURN_FALSE;
			}
			break;
		case 4:
			/* 4个参数 */
			if (Z_TYPE_P(args) != IS_ARRAY) {
				/* 第四个参数为Parameter，不为数组就报错 */
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Parameters must be an array");
				RETURN_FALSE;
			}
			/**
			 *	$parameters = $args;
			 *	Yaf_Request_*::$module = $module
			 *	Yaf_Request_*::$controller = $controller
			 *	Yaf_Request_*::$action = $action
			 *	Yaf_Request_*::$params = $parameters
			 */
			zend_hash_copy(Z_ARRVAL_P(parameters), Z_ARRVAL_P(args), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
			zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_MODULE), module TSRMLS_CC);
			zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), controller TSRMLS_CC);
			zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), action TSRMLS_CC);
			zend_update_property(request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), parameters TSRMLS_CC);
			break;
	}
	/* 设置分发标识Yaf_Request_Abstract::$dispatched = 0 */
	(void)yaf_request_set_dispatched(request, 0 TSRMLS_CC);
	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public Yaf_Controller_Abstract::redirect(string $url)
*/
PHP_METHOD(yaf_controller, redirect) {
	char 			*location;
	uint 			location_len;
	yaf_response_t 	*response;
	yaf_controller_t *self = getThis();

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &location, &location_len) == FAILURE) {
		return;
	}
	/* $response = $this->_response */
	response = zend_read_property(yaf_controller_ce, self, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_RESPONSE), 1 TSRMLS_CC);
	/* 设置redirect */
	(void)yaf_response_set_redirect(response, location, location_len TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */

/** {{{ proto protected Yaf_Controller_Abstract::render(string $action, array $var_array = NULL)
*/
PHP_METHOD(yaf_controller, render) {
	char *action_name;
	uint action_name_len;
	zval *var_array	= NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &action_name, &action_name_len, &var_array) == FAILURE) {
		return;
	} else {
		/* 获取渲染后的结果 */
		zval *output = yaf_controller_render(getThis(), action_name, action_name_len, var_array TSRMLS_CC);
		if (output) {
			if (IS_STRING == Z_TYPE_P(output)) {
				/* save a string copy here */
				ZVAL_STRINGL(return_value, Z_STRVAL_P(output), Z_STRLEN_P(output), 0);
				efree(output);
				return;
			} else {
				RETURN_ZVAL(output, 1, 1);
			}
		} else {
			RETURN_FALSE;
		}
	}
}
/* }}} */

/** {{{ proto protected Yaf_Controller_Abstract::display(string $action, array $var_array = NULL)
*/
PHP_METHOD(yaf_controller, display) {
	char *action_name;
	uint action_name_len;
	zval *var_array	= NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &action_name, &action_name_len, &var_array) == FAILURE) {
		return;
	} else {
		/* 渲染视图模板, 并直接输出渲染结果 */
		RETURN_BOOL(yaf_controller_display(getThis(), action_name, action_name_len, var_array TSRMLS_CC));
	}
}
/* }}} */

/** {{{ proto private Yaf_Controller_Abstract::__clone()
*/
PHP_METHOD(yaf_controller, __clone) {
}
/* }}} */

/** {{{ yaf_controller_methods
*/
zend_function_entry yaf_controller_methods[] = {
	PHP_ME(yaf_controller, render,	    yaf_controller_render_arginfo, 	ZEND_ACC_PROTECTED)
	PHP_ME(yaf_controller, display,	    yaf_controller_display_arginfo, ZEND_ACC_PROTECTED)
	PHP_ME(yaf_controller, getRequest,	yaf_controller_void_arginfo, 	ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, getResponse,	yaf_controller_void_arginfo, 	ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, getModuleName,yaf_controller_void_arginfo, 	ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, getView,		yaf_controller_void_arginfo, 	ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, initView,	yaf_controller_initview_arginfo,ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, setViewpath,	yaf_controller_setvdir_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, getViewpath,	yaf_controller_void_arginfo, 	ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, forward,	   	yaf_controller_forward_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, redirect,    yaf_controller_redirect_arginfo,ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, getInvokeArgs,yaf_controller_void_arginfo,   ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, getInvokeArg, yaf_controller_getiarg_arginfo,ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, __construct,	NULL, 							ZEND_ACC_CTOR|ZEND_ACC_FINAL|ZEND_ACC_PUBLIC)
	PHP_ME(yaf_controller, __clone, 	NULL, 							ZEND_ACC_PRIVATE|ZEND_ACC_FINAL)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
*/
YAF_STARTUP_FUNCTION(controller) {
	zend_class_entry ce;
	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Controller_Abstract", "Yaf\\Controller_Abstract", yaf_controller_methods);
	yaf_controller_ce = zend_register_internal_class_ex(&ce, NULL, NULL TSRMLS_CC);
	/* abstract Yaf_Controller_Abstract */
	yaf_controller_ce->ce_flags |= ZEND_ACC_EXPLICIT_ABSTRACT_CLASS;

	/**
	 *	public 	  $actions   	= null;
	 *	protected $_module   	= null;
	 *	protected $_name     	= null;
	 *	protected $_request  	= null;
	 *	protected $_response 	= null;
	 *	protected $_invoke_args = null;
	 *	protected $_view 		= null;
	 */
	zend_declare_property_null(yaf_controller_ce, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_ACTIONS),	ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_null(yaf_controller_ce, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_MODULE), 	ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_controller_ce, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_NAME), 	ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_controller_ce, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_REQUEST),	ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_controller_ce, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_RESPONSE),	ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_controller_ce, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_ARGS),		ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(yaf_controller_ce, ZEND_STRL(YAF_CONTROLLER_PROPERTY_NAME_VIEW),		ZEND_ACC_PROTECTED TSRMLS_CC);

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
