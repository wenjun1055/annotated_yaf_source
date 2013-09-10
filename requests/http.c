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

/* $Id: http.c 327549 2012-09-09 03:02:48Z laruence $ */

#include "ext/standard/url.h"

static zend_class_entry * yaf_request_http_ce;

/** {{{ yaf_request_t * yaf_request_http_instance(yaf_request_t *this_ptr, char *request_uri, char *base_uri TSRMLS_DC)
*/
yaf_request_t * yaf_request_http_instance(yaf_request_t *this_ptr, char *request_uri, char *base_uri TSRMLS_DC) {
	yaf_request_t *instance;
	zval *method, *params, *settled_uri = NULL;

	/* 如果传入了已经初始化的类的实例则直接使用，没有的话就初始化一个yaf_request_http_ce的实例 */
	if (this_ptr) {
		instance = this_ptr;
	} else {
		MAKE_STD_ZVAL(instance);
		object_init_ex(instance, yaf_request_http_ce);
	}

	/**
	 *	BEGIN_EXTERN_C()
	 *	ifdef ZTS
	 *		define SG(v) TSRMG(sapi_globals_id, sapi_globals_struct *, v)
	 *		SAPI_API extern int sapi_globals_id;
	 *	else
	 *		define SG(v) (sapi_globals.v)
	 *		extern SAPI_API sapi_globals_struct sapi_globals;
	 *	endif
	 */

    MAKE_STD_ZVAL(method);
    /** 
     *	1.如果能从sapi中获取到request_method的话则直接将取到的request_method的值复制给method,
     *	2.如果sapi_module.name的前三个字符是cli的话则method的值为cli
     *	3.都不符合上面的情况的话则给method赋值为Unknow
     */
    if (SG(request_info).request_method) {
        ZVAL_STRING(method, (char *)SG(request_info).request_method, 1);
    } else if (strncasecmp(sapi_module.name, "cli", 3)) {
        ZVAL_STRING(method, "Unknow", 1);
    } else {
        ZVAL_STRING(method, "Cli", 1);
    }
    /* $this->method = $method */
	zend_update_property(yaf_request_http_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_METHOD), method TSRMLS_CC);
	zval_ptr_dtor(&method);

	if (request_uri) {
		/* 如果传入了request_uri,则将它的值直接给新生成的字符串settled_uri */
		MAKE_STD_ZVAL(settled_uri);
		ZVAL_STRING(settled_uri, request_uri, 1);
	} else {
		zval *uri;
		/* 整个do{}while(0)里面的所有操作都是在进行settled_uri值的查找并且赋值的操作 */
		do {
#ifdef PHP_WIN32
			/* check this first so IIS will catch */
			/* 1.判断$_SERVER['HTTP_X_REWRITE_URL']是否存在，存在的话就以它为settled_uri */
			uri = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("HTTP_X_REWRITE_URL") TSRMLS_CC);
			if (Z_TYPE_P(uri) != IS_NULL) {
				settled_uri = uri;
				break;
			}
			zval_ptr_dtor(&uri);

			/* IIS7 with URL Rewrite: make sure we get the unencoded url (double slash problem) */
			/* 2.判断$_SERVER['IIS_WasUrlRewritten']是否存在，然后进行一些列的判断来确定settled_uri */
			uri = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("IIS_WasUrlRewritten") TSRMLS_CC);
			if (Z_TYPE_P(uri) != IS_NULL) {
				/* Why:外面已经取过一次了，为什么不直接用，又来一次重复操作呢 */
				zval *rewrited = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("IIS_WasUrlRewritten") TSRMLS_CC);
				/* $_SERVER['UNENCODED_URL'] */
				zval *unencode = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("UNENCODED_URL") TSRMLS_CC);
				//找到一个bug
				//if (Z_TYPE_P(rewrited) = IS_LONG
				if (Z_TYPE_P(rewrited) == IS_LONG
						&& Z_LVAL_P(rewrited) == 1
						&& Z_TYPE_P(unencode) == IS_STRING
						&& Z_STRLEN_P(unencode) > 0) {
					settled_uri = uri;
				}
				break;
			}
			zval_ptr_dtor(&uri);
#endif
			/* 3.判断$_SERVER['PATH_INFO']是否存在，如果存在则赋值给settled_uri */
			uri = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("PATH_INFO") TSRMLS_CC);
			if (Z_TYPE_P(uri) != IS_NULL) {
				settled_uri = uri;
				break;
			}
			zval_ptr_dtor(&uri);

			/* 	4.判断$_SERVER['REQUEST_URI']是否存在
			 *	如果存在，则判断它的值的开头是否为http,如果是则认为$_SERVER['REQUEST_URI']的值是一个完整的链接，利用parse_url()进行分解url,并且将分解出来的path赋值给settled_uri；
			 *	如果存在，并且字符串不是以http开头，则判断字符串中是否有?，如果存在?则截取?前面的部分赋值给settled_uri,不存在?的话直接将$_SERVER['REQUEST_URI']赋值给settled_uri
			 */
			uri = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("REQUEST_URI") TSRMLS_CC);
			/* $_SERVER['REQUEST_URI']存在 */
			if (Z_TYPE_P(uri) != IS_NULL) {
				/* Http proxy reqs setup request uri with scheme and host [and port] + the url path, only use url path */
				/* uri以http开头 */
				if (strstr(Z_STRVAL_P(uri), "http") == Z_STRVAL_P(uri)) {

					/**
					 *	typedef struct php_url {
					 *		char *scheme;
					 *		char *user;
					 *		char *pass;
					 *		char *host;
					 *		unsigned short port;
					 *		char *path;
					 *		char *query;
					 *		char *fragment;
					 *	} php_url;
					 *
					 *	php_url_parse的解析跟function parse_url()一样
					 *	http://lxr.php.net/xref/PHP_5_4/ext/standard/url.c#97
					 */
					php_url *url_info = php_url_parse(Z_STRVAL_P(uri));
					zval_ptr_dtor(&uri);
					/* 解析成功，存在path，则将path的值赋给settled_uri */
					if (url_info && url_info->path) {
						MAKE_STD_ZVAL(settled_uri);
						ZVAL_STRING(settled_uri, url_info->path, 1);
					}
					/** 
					 *	释放php_url结构体中的每一项 
					 *	http://lxr.php.net/xref/PHP_5_4/ext/standard/url.c#42
					 */
					php_url_free(url_info);
				} else {
					char *pos  = NULL;
					/* 	判断$_SERVER['REQUEST_URI']中是否带了参数
					 *	如果带了?的话，直接将?前面的赋值给settled_uri
					 *	没有的话直接赋值给settled_uri 
					 */
					if ((pos = strstr(Z_STRVAL_P(uri), "?"))) {
						MAKE_STD_ZVAL(settled_uri);
						ZVAL_STRINGL(settled_uri, Z_STRVAL_P(uri), pos - Z_STRVAL_P(uri), 1);
						zval_ptr_dtor(&uri);
					} else {
						settled_uri = uri;
					}
				}
				break;
			}
			zval_ptr_dtor(&uri);

			/**
			 *	5.判断$_SERVER['ORIG_PATH_INFO']是否存在，存在直接赋值给settled_uri
			 */
			uri = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("ORIG_PATH_INFO") TSRMLS_CC);
			if (Z_TYPE_P(uri) != IS_NULL) {
				/* intended do nothing */
				/* zval *query = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("QUERY_STRING") TSRMLS_CC);
				if (Z_TYPE_P(query) != IS_NULL) {
				}
				*/
				settled_uri = uri;
				break;
			}
			zval_ptr_dtor(&uri);

		} while (0);
	}

	/* 判断经过上面的直接输入或者层层解析是否得到了settled_uri */
	if (settled_uri) {
		/* 获取settled_uri结构体中字符串的值 */
		char *p = Z_STRVAL_P(settled_uri);
		/* 字符串的开头两个是//，则字符串指针往后移一位 */
		while (*p == '/' && *(p + 1) == '/') {
			p++;
		}
		/* 经过上面的判断后如果p所指的字符串跟settled_uri本身的字符串值不同时候，使用处理过后p的值替换掉原来的值，所以鸟哥才用了一个garbage吧 */
		if (p != Z_STRVAL_P(settled_uri)) {
			char *garbage = Z_STRVAL_P(settled_uri);
			ZVAL_STRING(settled_uri, p, 1);
			efree(garbage);
		}
		/* 将层层处理后得到的settled_uri赋值给$this->uri */
		zend_update_property(yaf_request_http_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_URI), settled_uri TSRMLS_CC);
		/* 设置base_uri */
		yaf_request_set_base_uri(instance, base_uri, Z_STRVAL_P(settled_uri) TSRMLS_CC);
		zval_ptr_dtor(&settled_uri);
	}
	/* $this->params = array(); */
	MAKE_STD_ZVAL(params);
	array_init(params);
	zend_update_property(yaf_request_http_ce, instance, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_PARAMS), params TSRMLS_CC);
	zval_ptr_dtor(&params);
	/* return $this; */
	return instance;
}
/* }}} */

/** {{{ proto public Yaf_Request_Http::getQuery(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_http, Query, 	YAF_GLOBAL_VARS_GET);
/* }}} */

/** {{{ proto public Yaf_Request_Http::getPost(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_http, Post,  	YAF_GLOBAL_VARS_POST);
/* }}} */

/** {{{ proto public Yaf_Request_Http::getRequet(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_http, Request, YAF_GLOBAL_VARS_REQUEST);
/* }}} */

/** {{{ proto public Yaf_Request_Http::getFiles(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_http, Files, 	YAF_GLOBAL_VARS_FILES);
/* }}} */

/** {{{ proto public Yaf_Request_Http::getCookie(mixed $name, mixed $default = NULL)
*/
YAF_REQUEST_METHOD(yaf_request_http, Cookie, 	YAF_GLOBAL_VARS_COOKIE);
/* }}} */

/** {{{ proto public Yaf_Request_Http::isXmlHttpRequest()
*/
PHP_METHOD(yaf_request_http, isXmlHttpRequest) {
	/* $_SERVER['HTTP_X_REQUESTED_WITH'] */
	zval * header = yaf_request_query(YAF_GLOBAL_VARS_SERVER, ZEND_STRL("HTTP_X_REQUESTED_WITH") TSRMLS_CC);
	/* if ($_SERVER['HTTP_X_REQUESTED_WITH'] === 'XMLHttpRequest') */
	if (Z_TYPE_P(header) == IS_STRING
			&& strncasecmp("XMLHttpRequest", Z_STRVAL_P(header), Z_STRLEN_P(header)) == 0) {
		/* $_SERVER['HTTP_X_REQUESTED_WITH']的值为字符串，并且值为XMLHttpRequest，则返回true，否则返回false */
		zval_ptr_dtor(&header);
		RETURN_TRUE;
	}
	zval_ptr_dtor(&header);
	RETURN_FALSE;
}
/* }}} */

/** {{{ proto public Yaf_Request_Http::get(mixed $name, mixed $default)
 * params -> post -> get -> cookie -> server
 */
PHP_METHOD(yaf_request_http, get) {
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

/** {{{ proto public Yaf_Request_Http::__construct(string $request_uri, string $base_uri)
*/
PHP_METHOD(yaf_request_http, __construct) {
	char *request_uri = NULL;
	char *base_uri	  = NULL;
	int  rlen		  = 0;
	int  blen 		  = 0;

	yaf_request_t *self = getThis();
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ss", &request_uri, &rlen, &base_uri, &blen) == FAILURE) {
		/*
		 *	zval_dtor(self);
		 *	ZVAL_FALSE(obj);
		 */
		YAF_UNINITIALIZED_OBJECT(getThis());
		return;
	}

	(void)yaf_request_http_instance(self, request_uri, base_uri TSRMLS_CC);
}
/* }}} */

/** {{{ proto private Yaf_Request_Http::__clone
 */
PHP_METHOD(yaf_request_http, __clone) {
}
/* }}} */

/** {{{ yaf_request_http_methods
 */
zend_function_entry yaf_request_http_methods[] = {
	PHP_ME(yaf_request_http, getQuery, 		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_http, getRequest, 	NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_http, getPost, 		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_http, getCookie,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_http, getFiles,		NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_http, get,			NULL, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_http, isXmlHttpRequest,NULL,ZEND_ACC_PUBLIC)
	PHP_ME(yaf_request_http, __construct,	NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(yaf_request_http, __clone,		NULL, ZEND_ACC_PRIVATE|ZEND_ACC_CLONE)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
 */
YAF_STARTUP_FUNCTION(request_http){
	zend_class_entry ce;
	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Request_Http", "Yaf\\Request\\Http", yaf_request_http_methods);
	/* class Yaf_Request_Http extends Yaf_Request_Abstract */
	yaf_request_http_ce = zend_register_internal_class_ex(&ce, yaf_request_ce, NULL TSRMLS_CC);

	/**
	 *	constant SCHEME_HTTP = 'http';
	 *	constant SCHEME_HTTPS = 'https';
	 */
	zend_declare_class_constant_string(yaf_request_ce, ZEND_STRL("SCHEME_HTTP"), "http" TSRMLS_CC);
	zend_declare_class_constant_string(yaf_request_ce, ZEND_STRL("SCHEME_HTTPS"), "https" TSRMLS_CC);

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
