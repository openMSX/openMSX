// $Id$

#include <cassert>
#include <algorithm>	// for tolower
#include <cstdlib>	// for strtol() and atoll()
#include "xmlx.hh"
#include "config.h"	// for the autoconf defines
#include "Config.hh"
#include "FileContext.hh"


namespace openmsx {

// class Config

Config::Config(XMLElement* element_, FileContext& context_)
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

XMLElement* Config::getParameterElement(const string& name) const
{
	if (element) {
		const XMLElement::Children& children = element->getChildren();
		for (XMLElement::Children::const_iterator it = children.begin();
		     it != children.end(); ++it) {
			if ((*it)->getName() == "parameter") {
				if ((*it)->getAttribute("name") == name) {
					return *it;
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
	XMLElement* p = getParameterElement(name);
	if (!p) {
		throw ConfigException("Missing parameter: " + name);
	}
	return p->getPcData();
}

const string Config::getParameter(const string& name, const string& defaultValue) const
// don't return reference!
{
	XMLElement* p = getParameterElement(name);
	return p ? p->getPcData() : defaultValue;
}

bool Config::getParameterAsBool(const string& name) const
	throw(ConfigException)
{
	return stringToBool(getParameter(name));
}

bool Config::getParameterAsBool(const string& name, bool defaultValue) const
{
	XMLElement* p = getParameterElement(name);
	return p ? stringToBool(p->getPcData()) : defaultValue;
}

int Config::getParameterAsInt(const string& name) const
	throw(ConfigException)
{
	XMLElement* p = getParameterElement(name);
	if (!p) {
		throw ConfigException("Missing parameter: " + name);
	}
	return stringToInt(getParameter(name));
}

int Config::getParameterAsInt(const string& name, int defaultValue) const
{
	XMLElement* p = getParameterElement(name);
	return p ? stringToInt(p->getPcData()) : defaultValue;
}

void Config::getParametersWithClass(const string& clasz, Parameters& result)
{
	if (element) {
		const XMLElement::Children& children = element->getChildren();
		for (XMLElement::Children::const_iterator it = children.begin();
		     it != children.end(); ++it) {
			if ((*it)->getName() == "parameter") {
				if ((*it)->getAttribute("class") == clasz) {
					Parameter parameter((*it)->getAttribute("name"),
					                    (*it)->getPcData(), clasz);
					result.push_back(parameter);
				}
			}
		}
	}
}


// class Parameter

Config::Parameter::Parameter(const string& name_,
                             const string& value_,
                             const string& clasz_)
	: name(name_), value(value_), clasz(clasz_)
{
}

bool Config::Parameter::getAsBool() const
{
	return stringToBool(value);
}

int Config::Parameter::getAsInt() const
{
	return stringToInt(value);
}


bool Config::stringToBool(const string& str)
{
	string low = str;
	transform(low.begin(), low.end(), low.begin(), ::tolower);
	return (low == "true" || low == "yes");
}

int Config::stringToInt(const string& str)
{
	// strtol also understands hex
	return strtol(str.c_str(), 0, 0);
}

} // namespace openmsx
