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

Config::Config(XML::Element *element_, const std::string &context_)
	: element(element_), context(context_)
{
}

Config::~Config()
{
}

const std::string &Config::getType() const
{
	return element->getElementPcdata("type");
}

const std::string &Config::getId() const
{
	return element->getAttribute("id");
}

const FileContext& Config::getContext() const
{
	return context;
}

XML::Element* Config::getParameterElement(const std::string &name) const
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


bool Config::hasParameter(const std::string &name) const
{
	XML::Element* p = getParameterElement(name);
	return (p != 0);
}

const std::string &Config::getParameter(const std::string &name) const
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return p->pcdata;
	}
	throw ConfigException("Missing parameter: " + name);
}

const bool Config::getParameterAsBool(const std::string &name) const
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return Config::Parameter::stringToBool(p->pcdata);
	}
	throw ConfigException("Missing parameter: " + name);
}

const int Config::getParameterAsInt(const std::string &name) const
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return Config::Parameter::stringToInt(p->pcdata);
	}
	throw ConfigException("Missing parameter: " + name);
}

const uint64 Config::getParameterAsUint64(const std::string &name) const
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

Device::Device(XML::Element *element, const std::string &context)
	: Config(element, context)
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

// class Slotted

Device::Slotted::Slotted(int PS, int SS, int Page)
:ps(PS),ss(SS),page(Page)
{
}

Device::Slotted::~Slotted()
{
}

bool Device::Slotted::hasSS() const
{
	return (ps != -1);
}

bool Device::Slotted::hasPage() const
{
	return (page != -1);
}

int Device::Slotted::getPS() const
{
	return ps;
}

int Device::Slotted::getSS() const
{
	if (ss == -1) {
		throw ConfigException("Request for SS on a Slotted without SS");
	}
	return ss;
}

int Device::Slotted::getPage() const
{
	if (page == -1) {
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

void MSXConfig::loadFile(const FileContext &context,
                         const std::string &filename)
{
	File file(context, filename);
	XML::Document* doc = new XML::Document(file.getLocalName());

	std::string base;
	unsigned int pos = filename.find_last_of('/');
	if (pos != std::string::npos) {
		base = filename.substr(0, pos + 1);
	}
	handleDoc(doc, base);
}

void MSXConfig::loadStream(const std::string &context,
                           const std::ostringstream &stream)
{
	XML::Document* doc = new XML::Document(stream);
	handleDoc(doc, context);
}

void MSXConfig::handleDoc(XML::Document* doc, const std::string &context)
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
				configs.push_back(new Config(*i, context));
			} else if ((*i)->name=="device") {
				devices.push_back(new Device(*i, context));
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

