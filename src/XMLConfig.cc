// $Id$

#include "XMLConfig.hh"

namespace XMLConfig
{

Config::Config(XML::Element *element_)
:element(element_)
{
}

Device::Device(XML::Element *element)
:XMLConfig::Config(element)
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

Device::~Device()
{
}

Backend::Backend()
{
}

Backend::~Backend()
{
	for (std::list<XML::Document*>::iterator doc = docs.begin(); doc != docs.end(); doc++)
	{
		delete (*doc);
	}
}

void Backend::loadFile(const std::string &filename)
{
	XML::Document* doc = new XML::Document(filename);
	docs.push_back(doc);
}

void Backend::saveFile()
{
	// TODO XXX
	assert(false);
}

void Backend::saveFile(const std::string &filename)
{
	// TODO XXX
	assert(false);
}

MSXConfig::Config* Backend::getConfigById(const std::string &type)
{
	return 0;
}


}; // end namespace XMLConfig
