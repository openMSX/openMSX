// $Id$

#include <cassert>
#include <string>
#include <algorithm>	// for tolower
#include <cstdlib>	// for strtol() and atoll()
#include "MSXConfig.hh"
#include "config.h"	// for the autoconf defines
#include "File.hh"

#ifndef HAVE_ATOLL
extern "C" long long atoll(const char *nptr);
#endif


namespace openmsx {

// class Config

Config::Config(XML::Element *element_, FileContext& context_)
	: element(element_), context(context_.clone())
{
}

Config::~Config()
{
	delete context;
}

const string &Config::getType() const
{
	return element->getElementPcdata("type");
}

const string &Config::getId() const
{
	return element->getAttribute("id");
}

FileContext& Config::getContext() const
{
	return *context;
}

XML::Element* Config::getParameterElement(const string &name) const
{
	list<XML::Element*>::iterator i;
	for (i = element->children.begin(); i != element->children.end(); i++) {
		if ((*i)->name == "parameter") {
			if ((*i)->getAttribute("name") == name) {
				return (*i);
			}
		}
	}
	return 0;
}


bool Config::hasParameter(const string &name) const
{
	XML::Element* p = getParameterElement(name);
	return (p != 0);
}

const string &Config::getParameter(const string &name) const
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return p->pcdata;
	}
	throw ConfigException("Missing parameter: " + name);
}

const string Config::getParameter(const string &name, const string &defaultValue) const
// don't return reference!
{
	XML::Element* p = getParameterElement(name);
	return p ? p->pcdata : defaultValue;
}

const bool Config::getParameterAsBool(const string &name) const
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return Config::Parameter::stringToBool(p->pcdata);
	}
	throw ConfigException("Missing parameter: " + name);
}

const bool Config::getParameterAsBool(const string &name, bool defaultValue) const
{
	XML::Element* p = getParameterElement(name);
	return p ? Config::Parameter::stringToBool(p->pcdata) : defaultValue;
}

const int Config::getParameterAsInt(const string &name) const
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return Config::Parameter::stringToInt(p->pcdata);
	}
	throw ConfigException("Missing parameter: " + name);
}

const int Config::getParameterAsInt(const string &name, int defaultValue) const
{
	XML::Element* p = getParameterElement(name);
	return p ? Config::Parameter::stringToInt(p->pcdata) : defaultValue;
}

const uint64 Config::getParameterAsUint64(const string &name) const
{
	XML::Element* p = getParameterElement(name);
	if (p != 0) {
		return Config::Parameter::stringToUint64(p->pcdata);
	}
	throw ConfigException("Missing parameter: " + name);
}

const uint64 Config::getParameterAsUint64(const string &name, const uint64 &defaultValue) const
{
	XML::Element* p = getParameterElement(name);
	return p ? Config::Parameter::stringToUint64(p->pcdata) : defaultValue;
}

list<Config::Parameter*>* Config::getParametersWithClass(const string &clasz)
{
	list<Config::Parameter*>* l = new list<Config::Parameter*>;
	list<XML::Element*>::const_iterator i;
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

Config::Parameter::Parameter(const string &name_,
	const string &value_,
	const string &clasz_)
	: name(name_), value(value_), clasz(clasz_)
{
}

Config::Parameter::~Parameter()
{
}

bool Config::Parameter::stringToBool(const string &str)
{
	string low = str;
	transform (low.begin(), low.end(), low.begin(), ::tolower);
	return (low == "true" || low == "yes");
}

int Config::Parameter::stringToInt(const string &str)
{
	// strtol also understands hex
	return strtol(str.c_str(), 0, 0);
}

uint64 Config::Parameter::stringToUint64(const string &str)
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

Device::Device(XML::Element *element_, FileContext &context_)
	: Config(element_, context_)
{
	// TODO: create slotted-eds ???
	list<XML::Element*>::iterator i;
	for (i = element->children.begin(); i != element->children.end(); i++) {
		if ((*i)->name == "slotted") {
			int PS = -2;
			int SS = -1;
			int Page = -1;
			list<XML::Element*>::iterator j;
			for (j = (*i)->children.begin(); j != (*i)->children.end(); j++) {
				if ((*j)->name == "ps") {
					PS = Config::Parameter::stringToInt((*j)->pcdata);
				} else if ((*j)->name == "ss") {
					SS = Config::Parameter::stringToInt((*j)->pcdata);
				} else if ((*j)->name == "page") {
					Page = Config::Parameter::stringToInt((*j)->pcdata);
				}
			}
			if (PS != -2) {
				slotted.push_back(new Device::Slotted(PS,SS,Page));
			}
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

int Device::Slotted::getPS() const
{
	return ps;
}

int Device::Slotted::getSS() const
{
	return ss;
}

int Device::Slotted::getPage() const
{
	return page;
}


// class MSXConfig

MSXConfig::MSXConfig()
{
}

MSXConfig::~MSXConfig()
{
	list<XML::Document*>::iterator doc;
	for (doc = docs.begin(); doc != docs.end(); doc++) {
		delete (*doc);
	}
	list<Config*>::iterator i;
	for (i = configs.begin(); i != configs.end(); i++) {
		delete (*i);
	}
	list<Device*>::iterator j;
	for (j = devices.begin(); j != devices.end(); j++) {
		delete (*j);
	}
}

MSXConfig* MSXConfig::instance()
{
	static MSXConfig oneInstance;
	return &oneInstance;
}

void MSXConfig::loadHardware(FileContext &context,
                             const string &filename)
{
	File file(context.resolve(filename));
	XML::Document *doc = new XML::Document(file.getLocalName());

	// get url
	string url(file.getURL());
	unsigned pos = url.find_last_of('/');
	assert (pos != string::npos);	// protocol must contain a '/'
	url = url.substr(0, pos);
	PRT_DEBUG("Hardware config: url "<<url);
	
	// TODO read hwDesc from config ???
	string hwDesc;
	pos = url.find_last_of('/');
	if (pos == string::npos) {
		hwDesc = "noname";
	} else {
		hwDesc = url.substr(pos + 1);
	}

	// TODO get user name
	string userName;
	
	ConfigFileContext context2(url + '/', hwDesc, userName);
	handleDoc(doc, context2);
}

void MSXConfig::loadSetting(FileContext &context,
                            const string &filename)
{
	File file(context.resolve(filename));
	XML::Document *doc = new XML::Document(file.getLocalName());
	SettingFileContext context2(file.getURL());
	handleDoc(doc, context2);
}

void MSXConfig::loadStream(FileContext &context,
                           const ostringstream &stream)
{
	XML::Document* doc = new XML::Document(stream);
	handleDoc(doc, context);
}

void MSXConfig::handleDoc(XML::Document* doc, FileContext &context)
{
	docs.push_back(doc);
	// TODO update/append Devices/Configs
	for (list<XML::Element*>::const_iterator i = doc->root->children.begin();
	     i != doc->root->children.end();
	     ++i) {
		if (((*i)->name == "config") || ((*i)->name == "device")) {
			string id((*i)->getAttribute("id"));
			if (id == "") {
				throw ConfigException(
					"<config> or <device> is missing 'id'");
			}
			if (hasConfigWithId(id)) {
				throw ConfigException(
				    "<config> or <device> with duplicate 'id':" + id);
			}
			if ((*i)->name == "config") {
				configs.push_back(new Config(*i, context));
			} else if ((*i)->name == "device") {
				devices.push_back(new Device(*i, context));
			}
		}
	}
}

void MSXConfig::saveFile()
{
	// TODO
	assert(false);
}

void MSXConfig::saveFile(const string &filename)
{
	// TODO
	assert(false);
}

bool MSXConfig::hasConfigWithId(const string &id)
{
	return findConfigById(id);
}

Config* MSXConfig::getConfigById(const string &id)
{
	Config* result = findConfigById(id);
	if (!result) {
		throw ConfigException("<config> or <device> with id:" + id +
		                      " not found");
	}
	return result;
}

Config* MSXConfig::findConfigById(const string &id)
{
	for (list<Config*>::const_iterator i = configs.begin();
	     i != configs.end();
	     ++i) {
		if ((*i)->getId() == id) {
			return (*i);
		}
	}
	for (list<Device*>::const_iterator i = devices.begin();
	     i != devices.end();
	     ++i) {
		if ((*i)->getId() == id) {
			return (*i);
		}
	}
	return NULL;
}

Device* MSXConfig::getDeviceById(const string &id)
{
	for (list<Device*>::const_iterator i = devices.begin();
	     i != devices.end();
	     ++i) {
		if ((*i)->getId() == id) {
			return (*i);
		}
	}
	throw ConfigException("<device> with id:" + id + " not found");
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

void Config::getParametersWithClassClean(list<Parameter*>* lst)
{
	list<Parameter*>::iterator i;
	for (i = lst->begin(); i != lst->end(); i++) {
		delete (*i);
	}
	delete lst;
}

} // namespace openmsx
