// $Id$

#include "XMLConfig.hh"

namespace XMLConfig
{

Config::Config()
{
}

Config::~Config()
{
}

const std::string &Config::getType()
{
	static std::string dummy("");
	return dummy;
}

const std::string &Config::getId()
{
	static std::string dummy("");
	return dummy;
}

Device::Device()
{
}

Device::~Device()
{
}

Backend::Backend()
{
}

Backend::~Backend()
{
}

void Backend::loadFile(const std::string &filename)
{
}

void Backend::saveFile()
{
}

void Backend::saveFile(const std::string &filename)
{
}

MSXConfig::Config* Backend::getConfigById(const std::string &type)
{
	return 0;
}


}; // end namespace XMLConfig
