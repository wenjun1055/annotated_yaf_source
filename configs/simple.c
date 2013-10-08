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

/* $Id: simple.c 327626 2012-09-13 02:57:39Z laruence $ */

zend_class_entry *yaf_config_simple_ce;

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(yaf_config_simple_construct_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, config_file)
	ZEND_ARG_INFO(0, section)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_config_simple_get_arginfo, 0, 0, 0)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_config_simple_rget_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_config_simple_set_arginfo, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_config_simple_isset_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(yaf_config_simple_unset_arginfo, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()
/* }}} */

/** {{{ yaf_config_t * yaf_config_simple_instance(yaf_config_t *this_ptr, zval *values, zval *readonly TSRMLS_DC)
*/
yaf_config_t * yaf_config_simple_instance(yaf_config_t *this_ptr, zval *values, zval *readonly TSRMLS_DC) {
	yaf_config_t *instance;

	switch (Z_TYPE_P(values)) {
		/* 传入的值的类型为数组 */
		case IS_ARRAY:
			if (this_ptr) {
				instance = this_ptr;
			} else {
				/* 初始并实例化一个对象yaf_config_simple_ce的实例 */
				MAKE_STD_ZVAL(instance);
				object_init_ex(instance, yaf_config_simple_ce);
			}
			/* 将value的值更新到对象属性$_config中 */
			zend_update_property(yaf_config_simple_ce, instance, ZEND_STRL(YAF_CONFIG_PROPERT_NAME), values TSRMLS_CC);
			if (readonly) {
				/* 强制类型转换 */
				convert_to_boolean(readonly);
				/* 将readonly转换后的值更新到对象属性$_readonly */
				zend_update_property_bool(yaf_config_simple_ce, instance, ZEND_STRL(YAF_CONFIG_PROPERT_NAME_READONLY), Z_BVAL_P(readonly) TSRMLS_CC);
			}
			/* 返回当前类的实例 */
			return instance;
		break;
		default:
			/* 传入的参数不为数组，报错 */
			yaf_trigger_error(YAF_ERR_TYPE_ERROR TSRMLS_CC, "Invalid parameters provided, must be an array");
			return NULL;
	}
}
/* }}} */

/** {{{ zval * yaf_config_simple_format(yaf_config_t *instance, zval **ppzval TSRMLS_DC)
 */
zval * yaf_config_simple_format(yaf_config_t *instance, zval **ppzval TSRMLS_DC) {
	zval *readonly, *ret;
	readonly = zend_read_property(yaf_config_simple_ce, instance, ZEND_STRL(YAF_CONFIG_PROPERT_NAME_READONLY), 1 TSRMLS_CC);
	ret = yaf_config_simple_instance(NULL, *ppzval, readonly TSRMLS_CC);
	return ret;
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::__construct(mixed $array, string $readonly)
*/
PHP_METHOD(yaf_config_simple, __construct) {
	zval *values, *readonly = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &values, &readonly) == FAILURE) {
		zval *prop;
		/* 将prop初始化为一个数组 */
		MAKE_STD_ZVAL(prop);
		array_init(prop);
		/* 将prop这个空数组更新到当前类的属性$_configz中 */
		zend_update_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), prop TSRMLS_CC);
		zval_ptr_dtor(&prop);

		/* 返回空 */
		return;
	}
	/* 如果传入了参数做响应的初始化工作 */
	(void)yaf_config_simple_instance(getThis(), values, readonly TSRMLS_CC);
}
/** }}} */

/** {{{ proto public Yaf_Config_Simple::get(string $name = NULL)
*/
PHP_METHOD(yaf_config_simple, get) {
	zval *ret, **ppzval;
	char *name;
	uint len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &name, &len) == FAILURE) {
		return;
	}

	if (!len) {
		/* 如果传输的参数的长度为0，也就是传入的name无效，那么直接返回对象本身 */
		RETURN_ZVAL(getThis(), 1, 0);
	} else {
		zval *properties;
		HashTable *hash;
		
		/* 获取$_config的值 */
		properties = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
		/* 获取数组值的指针 */
		hash  = Z_ARRVAL_P(properties);
		/* 以name为key在$_config里面查找响应的值 */
		if (zend_hash_find(hash, name, len + 1, (void **) &ppzval) == FAILURE) {
			/* 失败返回false */
			RETURN_FALSE;
		}
		/* 查找到的结果为一个数组 */
		if (Z_TYPE_PP(ppzval) == IS_ARRAY) {
			/* 以当前的数组值生成一个新的对象，并返回
			 * 实现：Yaf_Config_Ini::get('config')->params->url
			 */
			if ((ret = yaf_config_simple_format(getThis(), ppzval TSRMLS_CC))) {
				RETURN_ZVAL(ret, 1, 1);
			} else {
				RETURN_NULL();
			}
		} else {
			RETURN_ZVAL(*ppzval, 1, 0);
		}
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::toArray(void)
*/
PHP_METHOD(yaf_config_simple, toArray) {
	/* 获取$_config并直接返回 */
	zval *properties = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
	RETURN_ZVAL(properties, 1, 0);
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::set($name, $value)
*/
PHP_METHOD(yaf_config_simple, set) {
	/* 获取$_readonly的值 */
	zval *readonly = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME_READONLY), 1 TSRMLS_CC);

	if (!Z_BVAL_P(readonly)) {
		/* $_readonly=false 的情况 */

		zval *name, *value, *props;
		/* 接收$key,$value */
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz", &name, &value) == FAILURE) {
			return;
		}

		/* 发现bug，这里是检验key是否合法，也就是key应该是一个长度不为0的字符串 */
	//	if (Z_TYPE_P(name) != IS_STRING || Z_TYPE_P(name) != IS_STRING) {
		if (Z_TYPE_P(name) != IS_STRING || !Z_STRLEN_P(name)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expect a string key name");
			RETURN_FALSE;
		}
		/* value.refcount__gc++ */
		Z_ADDREF_P(value);
		/* 获取$_config的值 */
		props = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
		/* 将key,value这个键值对加入到$_config数组中 */
		if (zend_hash_update(Z_ARRVAL_P(props), Z_STRVAL_P(name), Z_STRLEN_P(name) + 1, (void **)&value, sizeof(zval*), NULL) == SUCCESS) {
			RETURN_TRUE;
		} else {
			Z_DELREF_P(value);
		}
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::__isset($name)
*/
PHP_METHOD(yaf_config_simple, __isset) {
	char *name;
	uint len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &len) == FAILURE) {
		return;
	} else {
		/* 获取$_config的值 */
		zval *prop = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
		/* 检查当前name是否在$_config这个数组的键之中 */
		RETURN_BOOL(zend_hash_exists(Z_ARRVAL_P(prop), name, len + 1));
	}
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::offsetUnset($index)
*/
PHP_METHOD(yaf_config_simple, offsetUnset) {
	/* 获取$_readonly的值 */
	zval *readonly = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME_READONLY), 1 TSRMLS_CC);

	if (!Z_BVAL_P(readonly)) {
		/* $_readonly=false 的情况 */

		zval *name, *props;
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &name) == FAILURE) {
			return;
		}

		/* 同上的bug，这里是检验key是否合法，也就是key应该是一个长度不为0的字符串 */
		//if (Z_TYPE_P(name) != IS_STRING || Z_TYPE_P(name) != IS_STRING) {
		if (Z_TYPE_P(name) != IS_STRING || !Z_STRLEN_P(name)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Expect a string key name");
			RETURN_FALSE;
		}

		/* 获取$_config的值 */
		props = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
		/* 删除$_config数组中以name的值为key的键值对 */
		if (zend_hash_del(Z_ARRVAL_P(props), Z_STRVAL_P(name), Z_STRLEN_P(name) + 1) == SUCCESS) {
			RETURN_TRUE;
		}
	}

	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::count($name)
*/
PHP_METHOD(yaf_config_simple, count) {
	/* 获取$_config的值 */
	zval *prop = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
	/* count($_config)并返回 */
	RETURN_LONG(zend_hash_num_elements(Z_ARRVAL_P(prop)));
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::rewind(void)
*/
PHP_METHOD(yaf_config_simple, rewind) {
	/* 获取$_config的值 */
	zval *prop = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
	/* 重置数组内指针 */
	zend_hash_internal_pointer_reset(Z_ARRVAL_P(prop));
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::current(void)
*/
PHP_METHOD(yaf_config_simple, current) {
	zval *prop, **ppzval, *ret;
	/* 获取$_config的值 */
	prop = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
	/* 获取数组$_config的内部指针当前指向的值 */
	if (zend_hash_get_current_data(Z_ARRVAL_P(prop), (void **)&ppzval) == FAILURE) {
		RETURN_FALSE;
	}

	if (Z_TYPE_PP(ppzval) == IS_ARRAY) {
		/* 如果值为数组的话则用数组内的值再次生成一个对象，并返回对象 */
		if ((ret = yaf_config_simple_format(getThis(), ppzval TSRMLS_CC))) {
			RETURN_ZVAL(ret, 1, 1);
		} else {
			RETURN_NULL();
		}
	} else {
		RETURN_ZVAL(*ppzval, 1, 0);
	}
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::key(void)
*/
PHP_METHOD(yaf_config_simple, key) {
	zval *prop;
	char *string;
	ulong index;
	/* 获取$_config的值 */
	prop = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
	/* 获取数组$_config的内部指针当前指向的键 */
	zend_hash_get_current_key(Z_ARRVAL_P(prop), &string, &index, 0);
	/* 判断键的类型，分别以字符串或者数字返回 */
	switch(zend_hash_get_current_key_type(Z_ARRVAL_P(prop))) {
		case HASH_KEY_IS_LONG:
			RETURN_LONG(index);
			break;
		case HASH_KEY_IS_STRING:
			RETURN_STRING(string, 1);
			break;
		default:
			RETURN_FALSE;
	}
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::next(void)
*/
PHP_METHOD(yaf_config_simple, next) {
	/* 获取$_config的值 */
	zval *prop = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
	/* 将对象属性数组$_config的hash表内部指针往后移一位 */
	zend_hash_move_forward(Z_ARRVAL_P(prop));
	RETURN_TRUE;
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::valid(void)
*/
PHP_METHOD(yaf_config_simple, valid) {
	/* 获取$_config的值 */
	zval *prop = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME), 1 TSRMLS_CC);
	/* 判断对象属性$_config内部指针是否已经到了hash表的结尾 */
	RETURN_LONG(zend_hash_has_more_elements(Z_ARRVAL_P(prop)) == SUCCESS);
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::readonly(void)
*/
PHP_METHOD(yaf_config_simple, readonly) {
	/* 获取$_readonly的值 */
	zval *readonly = zend_read_property(yaf_config_simple_ce, getThis(), ZEND_STRL(YAF_CONFIG_PROPERT_NAME_READONLY), 1 TSRMLS_CC);
	/* 以boolean类型返回$_readonly的值 */
	RETURN_BOOL(Z_LVAL_P(readonly));
}
/* }}} */

/** {{{ proto public Yaf_Config_Simple::__destruct
*/
PHP_METHOD(yaf_config_simple, __destruct) {
}
/* }}} */

/** {{{ proto private Yaf_Config_Simple::__clone
*/
PHP_METHOD(yaf_config_simple, __clone) {
}
/* }}} */

/** {{{ yaf_config_simple_methods
*/
zend_function_entry yaf_config_simple_methods[] = {
	PHP_ME(yaf_config_simple, __construct, yaf_config_simple_construct_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	/* PHP_ME(yaf_config_simple, __destruct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR) */
	PHP_ME(yaf_config_simple, __isset, yaf_config_simple_isset_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, get, yaf_config_simple_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, set, yaf_config_simple_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, count, yaf_config_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, offsetUnset,	yaf_config_simple_unset_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, rewind, yaf_config_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, current, yaf_config_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, next,	yaf_config_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, valid, yaf_config_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, key, yaf_config_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, readonly,	yaf_config_void_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_config_simple, toArray, yaf_config_void_arginfo, ZEND_ACC_PUBLIC)
	/**
	 *	__set = set
	 *	__get = get
	 *	offsetGet = get
	 *	offsetExists = __isset
	 *	offsetSet = set
	 */
	PHP_MALIAS(yaf_config_simple, __set, set, yaf_config_simple_set_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(yaf_config_simple, __get, get, yaf_config_simple_get_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(yaf_config_simple, offsetGet, get, yaf_config_simple_rget_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(yaf_config_simple, offsetExists, __isset, yaf_config_simple_isset_arginfo, ZEND_ACC_PUBLIC)
	PHP_MALIAS(yaf_config_simple, offsetSet, set, yaf_config_simple_set_arginfo, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
*/
YAF_STARTUP_FUNCTION(config_simple) {
	zend_class_entry ce;

	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Config_Simple", "Yaf\\Config\\Simple", yaf_config_simple_methods);
	yaf_config_simple_ce = zend_register_internal_class_ex(&ce, yaf_config_ce, NULL TSRMLS_CC);

#ifdef HAVE_SPL
	/* final class  Yaf\\Config\\Simple implements Iterator,ArrayAccess,Countable */
	zend_class_implements(yaf_config_simple_ce TSRMLS_CC, 3, zend_ce_iterator, zend_ce_arrayaccess, spl_ce_Countable);
#else
	/* final class Yaf_Config_Simple implements Iterator,ArrayAccess */
	zend_class_implements(yaf_config_simple_ce TSRMLS_CC, 2, zend_ce_iterator, zend_ce_arrayaccess);
#endif
	/* protected $_readonly=false */
	zend_declare_property_bool(yaf_config_simple_ce, ZEND_STRL(YAF_CONFIG_PROPERT_NAME_READONLY), 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	/*定义类的类型为final*/
	yaf_config_simple_ce->ce_flags |= ZEND_ACC_FINAL_CLASS;

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
