// $Id$

#include "MSXConfig.hh"

#include <string>

// for tolower
#include <algorithm>

// for strtol() and atoll()
#include <cstdlib>

namespace MSXConfig
{

Config::Config()
{
}

Config::~Config()
{
}

Config::Parameter::Parameter(const std::string &name_, const std::string &value_, const std::string &clasz_)
:name(name_), value(value_), clasz(clasz_)
{
}

Config::Parameter::~Parameter()
{
}

const bool Config::Parameter::getAsBool() const
{
	std::string low = value;
	std::transform (low.begin(), low.end(), low.begin(), tolower);
	if (low == "true" || low == "yes") return true;
	return false;
}

const int Config::Parameter::getAsInt() const
{
	// strtol also understands hex
	return strtol(value.c_str(),0,0);
}

const uint64 Config::Parameter::getAsUint64() const
{
	return atoll(value.c_str());
}

Backend::Backend()
{
}

Backend::~Backend()
{
}

Backend* Backend::createBackend(const std::string &name)
{
	return 0;
}

}; // end namespace MSXConfig
