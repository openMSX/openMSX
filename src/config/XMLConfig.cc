// $Id$

#include "XMLConfig.hh"

// the implementations of customconfig:
#include "XMLFilePath.hh"
// add others here


namespace XMLConfig
{

Config::Config(XML::Element *element_)
:element(element_)
{
}

Device::Device(XML::Element *element)
:XMLConfig::Config(element)
{
	// TODO: create slotted-eds ???
	for (std::list<XML::Element*>::iterator i = element->children.begin(); i != element->children.end(); i++)
	{
		if ((*i)->name=="slotted")
		{
			int PS=-1;
			int SS=-1;
			int Page=-1;
			for (std::list<XML::Element*>::iterator j = (*i)->children.begin(); j != (*i)->children.end(); j++)
			{
				if ((*j)->name=="ps") PS = MSXConfig::Config::Parameter::stringToInt((*j)->pcdata);
				if ((*j)->name=="ss") SS = MSXConfig::Config::Parameter::stringToInt((*j)->pcdata);
				if ((*j)->name=="page") Page = MSXConfig::Config::Parameter::stringToInt((*j)->pcdata);
			}
			if (PS != -1) slotted.push_back(new MSXConfig::Device::Slotted(PS,SS,Page));
		}
	}
}

Config::~Config()
{
}

void Config::dump()
{
	MSXConfig::Config::dump();
	std::cout << "XMLConfig::Device" << std::endl;
	for (std::list<XML::Element*>::iterator i = element->children.begin(); i != element->children.end(); i++)
	{
		if ((*i)->name=="parameter")
		{
			std::cout <<  "    parameter name='" << (*i)->getAttribute("name") << "' class='" << (*i)->getAttribute("class") << "' value='" << (*i)->pcdata << "'" << std::endl;
		}
	}
}

void Device::dump()
{
	MSXConfig::Device::dump();
	XMLConfig::Config::dump();
	std::cout << "XMLConfig::Device" << std::endl;
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
	throw MSXConfig::Exception("Missing parameter: " + name);
}

const bool Config::getParameterAsBool(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0)
	{
		return MSXConfig::Config::Parameter::stringToBool(p->pcdata);
	}
	throw MSXConfig::Exception("Missing parameter: " + name);
}

const int Config::getParameterAsInt(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0)
	{
		return MSXConfig::Config::Parameter::stringToInt(p->pcdata);
	}
	throw MSXConfig::Exception("Missing parameter: " + name);
}

const uint64 Config::getParameterAsUint64(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0)
	{
		return MSXConfig::Config::Parameter::stringToUint64(p->pcdata);
	}
	throw MSXConfig::Exception("Missing parameter: " + name);
}

std::list<MSXConfig::Config::Parameter*>* Config::getParametersWithClass(const std::string &clasz)
{
	std::list<MSXConfig::Config::Parameter*>* l = new std::list<MSXConfig::Config::Parameter*>;
	for (std::list<XML::Element*>::const_iterator i = element->children.begin(); i != element->children.end(); i++)
	{
		if ((*i)->name=="parameter")
		{
			if ((*i)->getAttribute("class")==clasz)
			{
				l->push_back(new MSXConfig::Config::Parameter((*i)->getAttribute("name"), (*i)->pcdata, clasz));
			}
		}
	}
	return l;
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
	for (std::list<Config*>::iterator i = configs.begin(); i != configs.end(); i++)
	{
		delete (*i);
	}
	for (std::list<Device*>::iterator i = devices.begin(); i != devices.end(); i++)
	{
		delete (*i);
	}
	for (std::list<CustomConfig*>::iterator i = custom_configs.begin(); i != custom_configs.end(); i++)
	{
		delete (*i);
	}
}

void Backend::loadFile(const std::string &filename)
{
	XML::Document* doc = new XML::Document(filename);
	handleDoc(doc);
}

void Backend::loadStream(const std::ostringstream &stream)
{
	XML::Document* doc = new XML::Document(stream);
	handleDoc(doc);
}

void Backend::handleDoc(XML::Document* doc)
{
	docs.push_back(doc);
	// TODO XXX update/append Devices/Configs
	for (std::list<XML::Element*>::const_iterator i = doc->root->children.begin(); i != doc->root->children.end(); i++)
	{
		if ((*i)->name=="config" || (*i)->name=="device")
		{
			std::string id((*i)->getAttribute("id"));
			if (id=="")
			{
				throw MSXConfig::Exception("<config> or <device> is missing 'id'");
			}
			if (hasConfigWithId(id))
			{
				std::ostringstream s;
				s << "<config> or <device> with duplicate 'id':" << id;
				throw MSXConfig::Exception(s);
			}
			if ((*i)->name=="config")
			{
				configs.push_back(new Config((*i)));
			}
			else if ((*i)->name=="device")
			{
				devices.push_back(new Device((*i)));
			}
		}
		else
		{
			createCustomConfigByTag((*i)->name);
			CustomConfig *c = dynamic_cast<CustomConfig*>(getCustomConfigByTag((*i)->name));
			c->backendInit((*i));
		}
	}
}

bool Backend::hasConfigWithId(const std::string &id)
{
	try
	{
		getConfigById(id);
	}
	catch (MSXConfig::Exception &e)
	{
		return false;
	}
	return true;
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

MSXConfig::Config* Backend::getConfigById(const std::string &id)
{
	for (std::list<XMLConfig::Config*>::const_iterator i = configs.begin(); i != configs.end(); i++)
	{
		if ((*i)->getId()==id)
		{
			return (*i);
		}
	}

	for (std::list<XMLConfig::Device*>::const_iterator i = devices.begin(); i != devices.end(); i++)
	{
		if ((*i)->getId()==id)
		{
			return (*i);
		}
	}
	std::ostringstream s;
	s << "<config> or <device> with id:" << id << " not found";
	throw MSXConfig::Exception(s);
}

MSXConfig::Device* Backend::getDeviceById(const std::string &id)
{
	for (std::list<XMLConfig::Device*>::const_iterator i = devices.begin(); i != devices.end(); i++)
	{
		if ((*i)->getId()==id)
		{
			return (*i);
		}
	}
	// TODO XXX raise exception?
	std::ostringstream s;
	s << "<device> with id:" << id << " not found";
	throw MSXConfig::Exception(s);
}

MSXConfig::CustomConfig* Backend::getCustomConfigByTag(const std::string &tag)
{
	std::list<XMLConfig::CustomConfig*>::const_iterator i;
	for (i = custom_configs.begin(); i != custom_configs.end(); i++)
	{
		if ((*i)->getTag()==tag)
		{
			return (*i);
		}
	}
	// TODO XXX raise exception?
	std::ostringstream s;
	s << "<" << tag << "> tag not found";
	throw MSXConfig::Exception(s);
}

void Backend::createCustomConfigByTag(const std::string &tag)
{
	if (tag=="filepath")
	{
		custom_configs.push_back(new FilePath());
		return;
	}
	
	std::ostringstream s;
	s << "there is no handler for customconfig with tag: " << tag;
	throw MSXConfig::Exception(s);
}


void Backend::initDeviceIterator()
{
	device_iterator = devices.begin();
}

MSXConfig::Device* Backend::getNextDevice()
{
	if (device_iterator != devices.end())
	{
		MSXConfig::Device* t= (*device_iterator);
		++device_iterator;
		return t;
	}
	return 0;
}

CustomConfig::CustomConfig()
:MSXConfig::CustomConfig()
{
	PRT_DEBUG("XMLConfig::CustomConfig::CustomConfig()");
}

CustomConfig::~CustomConfig()
{
}

}; // end namespace XMLConfig
