// $Id$

#include <cassert>
#include <algorithm>	// for tolower
#include <cstdlib>	// for strtol() and atoll()
#include "config.h"	// for the autoconf defines
#include "Config.hh"
#include "FileContext.hh"


namespace openmsx {

// class Config

Config::Config(const XMLElement& element_, const FileContext& context_)
	: element(element_), context(context_.clone())
{
}

Config::~Config()
{
	delete context;
}

const string& Config::getType() const
{
	return element.getElementPcdata("type");
}

const string& Config::getId() const
{
	return element.getAttribute("id");
}

const XMLElement& Config::getXMLElement() const
{
	return element;
}

FileContext& Config::getContext() const
{
	assert(context);
	return *context;
}

XMLElement* Config::getParameterElement(const string& name) const
{
	const XMLElement::Children& children = element.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == "parameter") {
			if ((*it)->getAttribute("name") == name) {
				return *it;
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
	const XMLElement::Children& children = element.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == "parameter") {
			if ((*it)->getAttribute("class") == clasz) {
				result.insert(Parameters::value_type(
					(*it)->getAttribute("name"),
					(*it)->getPcData()));
			}
		}
	}
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
