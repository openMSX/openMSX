// $Id$


#include <string>
#include <algorithm>	// for tolower
#include <cstdlib>	// for strtol() and atoll()
#include "MSXConfig.hh"
#include "config.h"	// for the autoconf defines
#include "File.hh"

#ifndef HAVE_ATOLL
extern "C" long long atoll(const char *nptr);
#endif


// class Config

Config::Config(XML::Element *element_)
	: element(element_)
{
}

Config::~Config()
{
}

bool Config::isDevice()
{
	return false;
}

void Config::dump()
{
	std::cout << "Config" << std::endl;
	std::cout << "    type: " << getType() << std::endl;
	std::cout << "    id: " << getId() << std::endl;
	std::cout << "    desc: " << getDesc() << std::endl;
	std::cout << "    rem: " << getRem() << std::endl;
	
	std::cout << "Device" << std::endl;
	std::list<XML::Element*>::iterator i;
	for (i = element->children.begin(); i != element->children.end(); i++) {
		if ((*i)->name == "parameter") {
			std::cout << "    parameter name='" << (*i)->getAttribute("name") 
			          << "' class='" << (*i)->getAttribute("class")
				  << "' value='" << (*i)->pcdata << "'" << std::endl;
		}
	}
}


// class Parameter

Config::Parameter::Parameter(const std::string &name_,
                             const std::string &value_,
			     const std::string &clasz_)
	: name(name_), value(value_), clasz(clasz_)
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


// class Device

Device::Device(XML::Element *element)
	: Config(element)
{
	// TODO: create slotted-eds ???
	std::list<XML::Element*>::iterator i;
	for (i = element->children.begin(); i != element->children.end(); i++) {
		if ((*i)->name == "slotted") {
			int PS=-1;
			int SS=-1;
			int Page=-1;
			std::list<XML::Element*>::iterator j;
			for (j = (*i)->children.begin(); j != (*i)->children.end(); j++) {
				if ((*j)->name == "ps") 
					PS = Config::Parameter::stringToInt((*j)->pcdata);
				if ((*j)->name == "ss")
					SS = Config::Parameter::stringToInt((*j)->pcdata);
				if ((*j)->name == "page")
					Page = Config::Parameter::stringToInt((*j)->pcdata);
			}
			if (PS != -1) 
				slotted.push_back(new Device::Slotted(PS,SS,Page));
		}
	}
}

Device::~Device()
{
}

bool Device::isDevice()
{
	return true;
}

void Device::dump()
{
	std::cout << "Device" << std::endl;
	std::list<Slotted*>::const_iterator i;
	for (i = slotted.begin(); i != slotted.end(); i++) {
		(*i)->dump();
	}
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
	std::list<XML::Element*>::iterator i;
	for (i = element->children.begin(); i != element->children.end(); i++) {
		if ((*i)->name == "parameter") {
			if ((*i)->getAttribute("name") == name) {
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
	if (p != 0) {
		return p->pcdata;
	}
	throw ConfigException("Missing parameter: " + name);
}

const bool Config::getParameterAsBool(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return Config::Parameter::stringToBool(p->pcdata);
	}
	throw ConfigException("Missing parameter: " + name);
}

const int Config::getParameterAsInt(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return Config::Parameter::stringToInt(p->pcdata);
	}
	throw ConfigException("Missing parameter: " + name);
}

const uint64 Config::getParameterAsUint64(const std::string &name)
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return Config::Parameter::stringToUint64(p->pcdata);
	}
	throw ConfigException("Missing parameter: " + name);
}

std::list<Config::Parameter*>* Config::getParametersWithClass(const std::string &clasz)
{
	std::list<Config::Parameter*>* l = new std::list<Config::Parameter*>;
	std::list<XML::Element*>::const_iterator i;
	for (i = element->children.begin(); i != element->children.end(); i++) {
		if ((*i)->name == "parameter") {
			if ((*i)->getAttribute("class") == clasz) {
				l->push_back(new Config::Parameter((*i)->getAttribute("name"), (*i)->pcdata, clasz));
			}
		}
	}
	return l;
}

// class Slotted

Device::Slotted::Slotted(int PS, int SS, int Page)
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
		throw ConfigException("Request for SS on a Slotted without SS");
	}
	return ss;
}

int Device::Slotted::getPage()
{
	if (page==-1)
	{
		throw ConfigException("Request for Page on a Slotted without Page");
	}
	return page;
}


// class MSXConfig

MSXConfig::MSXConfig()
{
}

MSXConfig::~MSXConfig()
{
	std::list<XML::Document*>::iterator doc;
	for (doc = docs.begin(); doc != docs.end(); doc++) {
		delete (*doc);
	}
	std::list<Config*>::iterator i;
	for (i = configs.begin(); i != configs.end(); i++) {
		delete (*i);
	}
	std::list<Device*>::iterator j;
	for (j = devices.begin(); j != devices.end(); j++) {
		delete (*j);
	}
}

MSXConfig* MSXConfig::instance()
{
	static MSXConfig oneInstance;
	
	return &oneInstance;
}

void MSXConfig::loadFile(const std::string &filename)
{
	XML::Document* doc = new XML::Document(File::findName(filename, CONFIG));
	handleDoc(doc);
}

void MSXConfig::loadStream(const std::ostringstream &stream)
{
	XML::Document* doc = new XML::Document(stream);
	handleDoc(doc);
}

void MSXConfig::handleDoc(XML::Document* doc)
{
	docs.push_back(doc);
	// TODO update/append Devices/Configs
	std::list<XML::Element*>::const_iterator i;
	for (i = doc->root->children.begin(); i != doc->root->children.end(); i++) {
		if ((*i)->name=="config" || (*i)->name=="device") {
			std::string id((*i)->getAttribute("id"));
			if (id=="") {
				throw ConfigException("<config> or <device> is missing 'id'");
			}
			if (hasConfigWithId(id)) {
				std::ostringstream s;
				s << "<config> or <device> with duplicate 'id':" << id;
				throw ConfigException(s);
			}
			if ((*i)->name=="config") {
				configs.push_back(new Config((*i)));
			} else if ((*i)->name=="device") {
				devices.push_back(new Device((*i)));
			}
		}
	}
}

bool MSXConfig::hasConfigWithId(const std::string &id)
{
	try {
		getConfigById(id);
	} catch (ConfigException &e) {
		return false;
	}
	return true;
}

void MSXConfig::saveFile()
{
	// TODO
	assert(false);
}

void MSXConfig::saveFile(const std::string &filename)
{
	// TODO
	assert(false);
}

Config* MSXConfig::getConfigById(const std::string &id)
{
	std::list<Config*>::const_iterator i;
	for (i = configs.begin(); i != configs.end(); i++) {
		if ((*i)->getId()==id) {
			return (*i);
		}
	}

	std::list<Device*>::const_iterator j;
	for (j = devices.begin(); j != devices.end(); j++) {
		if ((*j)->getId()==id) {
			return (*j);
		}
	}
	std::ostringstream s;
	s << "<config> or <device> with id:" << id << " not found";
	throw ConfigException(s);
}

Device* MSXConfig::getDeviceById(const std::string &id)
{
	std::list<Device*>::const_iterator i;
	for (i = devices.begin(); i != devices.end(); i++) {
		if ((*i)->getId()==id) {
			return (*i);
		}
	}
	// TODO XXX raise exception?
	std::ostringstream s;
	s << "<device> with id:" << id << " not found";
	throw ConfigException(s);
}

void MSXConfig::initDeviceIterator()
{
	device_iterator = devices.begin();
}

Device* MSXConfig::getNextDevice()
{
	if (device_iterator != devices.end()) {
		Device* t= (*device_iterator);
		++device_iterator;
		return t;
	}
	return 0;
}
void Config::getParametersWithClassClean(std::list<Parameter*>* list)
{
	std::list<Parameter*>::iterator i;
	for (i = list->begin(); i != list->end(); i++) {
		delete (*i);
	}
	delete list;
}

