// $Id$

#include <cassert>
#include "Config.hh"
#include "FileContext.hh"
#include "StringOp.hh"

namespace openmsx {

// class Config

Config::Config(const XMLElement& element, const FileContext& context_)
	: XMLElement(element), context(context_.clone())
{
}

Config::~Config()
{
	delete context;
}

const string& Config::getType() const
{
	return getChildData("type");
}

const string& Config::getId() const
{
	return getAttribute("id");
}

FileContext& Config::getContext() const
{
	assert(context);
	return *context;
}

const XMLElement* Config::getParameterElement(const string& name) const
{
	const XMLElement* param = getChild(name);
	if (param) {
		return param;
	}

	XMLElement::Children params;
	getChildren("parameter", params);
	for (XMLElement::Children::const_iterator it = params.begin();
	     it != params.end(); ++it) {
		if ((*it)->getAttribute("name") == name) {
			return *it;
		}
	}
	return NULL;
}


bool Config::hasParameter(const string& name) const
{
	return getParameterElement(name);
}

const string& Config::getParameter(const string& name) const
{
	const XMLElement* p = getParameterElement(name);
	if (!p) {
		throw ConfigException("Missing parameter: " + name);
	}
	return p->getData();
}

const string Config::getParameter(const string& name, const string& defaultValue) const
// don't return reference!
{
	const XMLElement* p = getParameterElement(name);
	return p ? p->getData() : defaultValue;
}

bool Config::getParameterAsBool(const string& name) const
{
	return StringOp::stringToBool(getParameter(name));
}

bool Config::getParameterAsBool(const string& name, bool defaultValue) const
{
	const XMLElement* p = getParameterElement(name);
	return p ? StringOp::stringToBool(p->getData()) : defaultValue;
}

int Config::getParameterAsInt(const string& name) const
{
	return StringOp::stringToInt(getParameter(name));
}

int Config::getParameterAsInt(const string& name, int defaultValue) const
{
	const XMLElement* p = getParameterElement(name);
	return p ? StringOp::stringToInt(p->getData()) : defaultValue;
}

void Config::getParametersWithClass(const string& clasz, Parameters& result) const
{
	const XMLElement::Children& children = getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == "parameter") {
			if ((*it)->getAttribute("class") == clasz) {
				result.insert(Parameters::value_type(
					(*it)->getAttribute("name"),
					(*it)->getData()));
			}
		}
	}
}

} // namespace openmsx
