#include <iostream>
#include <string>

#include "ng_config.h"

config* config::getInstance()
{
	if (pinstance == nullptr)
		pinstance = new config();
	return pinstance;
}

config::config()
{
	
}

config::~config()
{

}

bool config::set(const std::string& k, std::string v)
{
	this->config_str[k] = v; //.insert( config_str_t::value_type(std::string(k), std::string(v)) );
	return true;
}

bool config::setInt(const std::string& k, int v)
{
	this->config_int[k] = v; //.insert( config_int_t::value_type(k, v) );
	return true;
}


bool config::get(const std::string& k, std::string *t)
{
	t->clear();
	config_str_t::iterator it;
	it = this->config_str.find( k );
	if (it != this->config_str.end()) {
		t->assign(it->second);
		return true;
	}
	return false;
}

bool config::getInt(const std::string& k, int *t)
{
	config_int_t::iterator it;
	it = this->config_int.find( k );
	if (it != this->config_int.end()) {
		*t = it->second;
		return true;
	}
	return false;
}

std::string config::get(const std::string& k, std::string def)
{
	config_str_t::iterator it;
	it = this->config_str.find( k );
	if (it != this->config_str.end()) {
		return it->second;
	}
	return def;
}

int config::getInt(const std::string& k, int def)
{
	config_int_t::iterator it;
	it = this->config_int.find( k );
	if (it != this->config_int.end()) {
		return it->second;
	}
	return def;
}
