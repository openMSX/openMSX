// $Id$

#include "MSXConfig.hh"

#include <string>

// for tolower
#include <algorithm>

// for strtol() and atoll()
#include <cstdlib>

// the backend[s]:
#include "XMLConfig.hh"

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

Backend* Backend::createBackend(const std::string &name)
{
	if (name=="xml")
	{
		return new XMLConfig::Backend();
	}
	return 0;
}

void Config::getParametersWithClassClean(std::list<Parameter*>* list)
{
	for (std::list<Parameter*>::iterator i = list->begin(); i != list->end(); i++)
	{
		delete (*i);
	}
	delete list;
}

}; // end namespace MSXConfig
