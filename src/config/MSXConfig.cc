// $Id$

#include "MSXConfig.hh"

#include <string>

// for tolower
#include <algorithm>

// for strtol() and atoll()
#include <cstdlib>

// the backend[s]:
#include "XMLConfig.hh"

// for the autoconf defines
#include "config.h"

#ifndef HAVE_ATOLL
extern "C" long long atoll(const char *nptr);
#endif

namespace MSXConfig
{

Config::Config()
{
}

Config::~Config()
{
}

bool Config::isDevice()
{
	return false;
}

Config::Parameter::Parameter(const std::string &name_, const std::string &value_, const std::string &clasz_)
:name(name_), value(value_), clasz(clasz_)
{
}

Config::Parameter::~Parameter()
{
}

bool Config::Parameter::stringToBool(const std::string &str)
{
	std::string low = str;
	std::transform (low.begin(), low.end(), low.begin(), tolower);
	return (low == "true" || low == "yes");
}

int Config::Parameter::stringToInt(const std::string &str)
{
	// strtol also understands hex
	return strtol(str.c_str(),0,0);
}

uint64 Config::Parameter::stringToUint64(const std::string &str)
{
	return atoll(str.c_str());
}

const bool Config::Parameter::getAsBool() const
{
	return stringToBool(value);
}

const int Config::Parameter::getAsInt() const
{
	return stringToInt(value);
}

const uint64 Config::Parameter::getAsUint64() const
{
	return stringToUint64(value);
}

Device::Device()
{
}

Device::~Device()
{
}

bool Device::isDevice()
{
	return true;
}

Device::Slotted::Slotted(int PS, int SS=-1, int Page=-1)
:ps(PS),ss(SS),page(Page)
{
}

void Device::Slotted::dump()
{
	std::cout << "        slotted: PS: " << ps << " SS: " << ss << " Page: " << page << std::endl;
}

Device::Slotted::~Slotted()
{
}

bool Device::Slotted::hasSS()
{
	return (ps!=-1);
}

bool Device::Slotted::hasPage()
{
	return (page!=-1);
}

int Device::Slotted::getPS()
{
	return ps;
}

int Device::Slotted::getSS()
{
	if (ss==-1)
	{
		throw Exception("Request for SS on a Slotted without SS");
	}
	return ss;
}

int Device::Slotted::getPage()
{
	if (page==-1)
	{
		throw Exception("Request for Page on a Slotted without Page");
	}
	return page;
}

Backend::Backend()
{
}

Backend::~Backend()
{
}

Backend* Backend::_instance;

Backend* Backend::createBackend(const std::string &name)
{
	if (name=="xml")
	{
		Backend::_instance = new XMLConfig::Backend();
		return Backend::_instance;
	}
	return 0;
}

Backend* Backend::instance()
{
	return _instance;
}

void Config::getParametersWithClassClean(std::list<Parameter*>* list)
{
	for (std::list<Parameter*>::iterator i = list->begin(); i != list->end(); i++)
	{
		delete (*i);
	}
	delete list;
}

void Config::dump()
{
	std::cout << "MSXConfig::Config" << std::endl;
	std::cout << "    type: " << getType() << std::endl;
	std::cout << "    id: " << getId() << std::endl;
	std::cout << "    desc: " << getDesc() << std::endl;
	std::cout << "    rem: " << getRem() << std::endl;
	// parameters have to be dumped by the backend
}

void Device::dump()
{
	//Config::dump();
	std::cout << "MSXConfig::Device" << std::endl;
	for (std::list<Slotted*>::const_iterator i = slotted.begin(); i != slotted.end(); i++)
	{
		(*i)->dump();
	}
}

CustomConfig::CustomConfig(const std::string &_tag):tag(_tag)
{
}

CustomConfig::~CustomConfig()
{
}

CustomConfig::CustomConfig()
{
}

const std::string &CustomConfig::getTag()
{
	return tag;
}

}; // end namespace MSXConfig
