// $Id$

#include <cassert>
#include <algorithm>	// for tolower
#include <cstdlib>	// for strtol() and atoll()
#include "config.h"	// for the autoconf defines
#include "Config.hh"
#include "FileContext.hh"

#ifndef HAVE_ATOLL
extern "C" long long atoll(const char* nptr);
#endif


namespace openmsx {

// class Config

Config::Config(XML::Element* element_, FileContext& context_)
	: element(element_), context(context_.clone()),
	  type(element->getElementPcdata("type")),
	  id(element->getAttribute("id"))
{
}

Config::Config(const string& type_, const string& id_)
	: element(0), context(0), type(type_), id(id_)
{
}

Config::~Config()
{
	delete context;
}

const string &Config::getType() const
{
	return type;
}

const string &Config::getId() const
{
	return id;
}

FileContext& Config::getContext() const
{
	assert(context);
	return *context;
}

XML::Element* Config::getParameterElement(const string& name) const
{
	if (element) {
		for (list<XML::Element*>::iterator i = element->children.begin();
		     i != element->children.end(); ++i) {
			if ((*i)->name == "parameter") {
				if ((*i)->getAttribute("name") == name) {
					return (*i);
				}
			}
		}
	}
	return NULL;
}


bool Config::hasParameter(const string& name) const
{
	return getParameterElement(name);
}

const string &Config::getParameter(const string& name) const
	throw(ConfigException)
{
	XML::Element* p = getParameterElement(name);
	if (!p) {
		throw ConfigException("Missing parameter: " + name);
	}
	return p->pcdata;
}

const string Config::getParameter(const string& name, const string& defaultValue) const
// don't return reference!
{
	XML::Element* p = getParameterElement(name);
	return p ? p->pcdata : defaultValue;
}

const bool Config::getParameterAsBool(const string& name) const
	throw(ConfigException)
{
	XML::Element* p = getParameterElement(name);
	if (!p) {
		throw ConfigException("Missing parameter: " + name);
	}
	return Config::Parameter::stringToBool(p->pcdata);
}

const bool Config::getParameterAsBool(const string& name, bool defaultValue) const
{
	XML::Element* p = getParameterElement(name);
	return p ? Config::Parameter::stringToBool(p->pcdata) : defaultValue;
}

const int Config::getParameterAsInt(const string& name) const
	throw(ConfigException)
{
	XML::Element* p = getParameterElement(name);
	if (!p) {
		throw ConfigException("Missing parameter: " + name);
	}
	return Config::Parameter::stringToInt(p->pcdata);
}

const int Config::getParameterAsInt(const string& name, int defaultValue) const
{
	XML::Element* p = getParameterElement(name);
	return p ? Config::Parameter::stringToInt(p->pcdata) : defaultValue;
}

const uint64 Config::getParameterAsUint64(const string& name) const
	throw(ConfigException)
{
	XML::Element* p = getParameterElement(name);
	if (!p) {
		throw ConfigException("Missing parameter: " + name);
	}
	return Config::Parameter::stringToUint64(p->pcdata);
}

const uint64 Config::getParameterAsUint64(const string& name, const uint64& defaultValue) const
{
	XML::Element* p = getParameterElement(name);
	return p ? Config::Parameter::stringToUint64(p->pcdata) : defaultValue;
}

list<Config::Parameter*>* Config::getParametersWithClass(const string& clasz)
{
	list<Config::Parameter*>* l = new list<Config::Parameter*>;
	if (element) {
		for (list<XML::Element*>::const_iterator i = element->children.begin();
		     i != element->children.end(); ++i) {
			if ((*i)->name == "parameter") {
				if ((*i)->getAttribute("class") == clasz) {
					l->push_back(new Parameter((*i)->getAttribute("name"),
								   (*i)->pcdata, clasz));
				}
			}
		}
	}
	return l;
}

void Config::getParametersWithClassClean(list<Parameter*>* lst)
{
	for (list<Parameter*>::iterator i = lst->begin();
	     i != lst->end(); ++i) {
		delete (*i);
	}
	delete lst;
}


// class Parameter

Config::Parameter::Parameter(const string& name_,
                             const string& value_,
                             const string& clasz_)
	: name(name_), value(value_), clasz(clasz_)
{
}

Config::Parameter::~Parameter()
{
}

bool Config::Parameter::stringToBool(const string& str)
{
	string low = str;
	transform (low.begin(), low.end(), low.begin(), ::tolower);
	return (low == "true" || low == "yes");
}

int Config::Parameter::stringToInt(const string& str)
{
	// strtol also understands hex
	return strtol(str.c_str(), 0, 0);
}

uint64 Config::Parameter::stringToUint64(const string& str)
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

} // namespace openmsx
