// $Id$

#include <cassert>
#include "Config.hh"
#include "FileContext.hh"
#include "StringOp.hh"
#include "ConfigException.hh"

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

bool Config::hasParameter(const string& name) const
{
	return getChild(name);
}

const string& Config::getParameter(const string& name) const
{
	const XMLElement* p = getChild(name);
	if (!p) {
		throw ConfigException("Missing parameter: " + name);
	}
	return p->getData();
}

const string Config::getParameter(const string& name, const string& defaultValue) const
// don't return reference!
{
	const XMLElement* p = getChild(name);
	return p ? p->getData() : defaultValue;
}

bool Config::getParameterAsBool(const string& name) const
{
	return StringOp::stringToBool(getParameter(name));
}

bool Config::getParameterAsBool(const string& name, bool defaultValue) const
{
	const XMLElement* p = getChild(name);
	return p ? StringOp::stringToBool(p->getData()) : defaultValue;
}

int Config::getParameterAsInt(const string& name) const
{
	return StringOp::stringToInt(getParameter(name));
}

int Config::getParameterAsInt(const string& name, int defaultValue) const
{
	const XMLElement* p = getChild(name);
	return p ? StringOp::stringToInt(p->getData()) : defaultValue;
}

} // namespace openmsx
