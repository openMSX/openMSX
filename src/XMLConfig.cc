// $Id$

#include "XMLConfig.hh"

namespace XMLConfig
{

static std::string empty("");

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
	return element->getElementPcdata("type");
}

const std::string &Config::getDesc()
{
	return element->getElementPcdata("desc");
}

const std::string &Config::getRem()
{
	return element->getElementPcdata("rem");
}

const std::string &Config::getId()
{
	return element->getAttribute("id");
}

XML::Element* Config::getParameterElement(const std::string &name)
{
	for (std::list<XML::Element*>::iterator i = element->children.begin(); i != element->children.end(); i++)
	{
		if ((*i)->name=="parameter")
		{
			if ((*i)->getAttribute("name")==name)
			{
				//return new Parameter(name, (*i)->pcdata, (*i)->getAttribute("class"));
				return (*i);
			}
		}
	}
	return 0;
}


bool Config::hasParameter(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	return (p != 0);
}

const std::string &Config::getParameter(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0)
	{
		return p->pcdata;
	}
	return empty;
}

const bool Config::getParameterAsBool(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0)
	{
		return MSXConfig::Config::Parameter::stringToBool(p->pcdata);
	}
	// TODO XXX exception?
	return false;
}

const int Config::getParameterAsInt(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0)
	{
		return MSXConfig::Config::Parameter::stringToInt(p->pcdata);
	}
	// TODO XXX exception?
	return 0;
}

const uint64 Config::getParameterAsUint64(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0)
	{
		return MSXConfig::Config::Parameter::stringToUint64(p->pcdata);
	}
	// TODO XXX exception?
	return 0L;
}

std::list<MSXConfig::Config::Parameter*>* Config::getParametersWithClass(const std::string &clasz)
{
	// TODO XXX
	return 0;
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
