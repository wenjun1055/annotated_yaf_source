<?php
final class Yaf_Registry
{
    protected static $_instance = null;

    protected $_entries = null;

    private function __construct()
    {
    }

    private function __clone()
    {   
    }

    public static function getInstance()
    {
        if (empty(self::$_instance) ||
            ! (self::$_instance instanceof Yaf_Registry)) {
            self::$_instance = new self();
            self::$_instance->_entries = array();
        }
            

        return self::$_instance;
            
    }

    public static function get($name)
    {
        if ($name) {
            $instance = self::getInstance(); 
            if (isset($instance->_entries[$name])) {
                return $instance->_entries[$name];
            }
        }

        return null;        
    }

    public static function set($name, $value)
    {
        if ($name && $value) {
            $instance = self::getInstance(); 
            $instance->_entries[$name] = $value;
            return true;
        }

        return false;
    }

    public static function has($name)
    {
        if ($name) {
            $instance = self::getInstance(); 
            return isset($instance->_entries[$name]);
        }

        return false;
    }

    public static function del($name)
    {
        if ($name) {
            $instance = self::getInstance(); 
            if (isset($instance->_entries[$name])) {
                unset($instance->_entries[$name]);
            }
        }

        return true;
    }
}


Yaf_Registry::set('key1', 'value1');
Yaf_Registry::set('key2', 'value2');
var_dump(Yaf_Registry::get('key1'));    //string(6) "value1"
var_dump(Yaf_Registry::get('key2'));    //string(6) "value2"
var_dump(Yaf_Registry::has('key1'));    //bool(true)
var_dump(Yaf_Registry::has('key2'));    //bool(true)
Yaf_Registry::del('key1');
var_dump(Yaf_Registry::has('key1'));    //bool(false)
var_dump(Yaf_Registry::has('key2'));    //bool(true)

?>