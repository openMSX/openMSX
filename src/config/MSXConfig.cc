// $Id$

#include <cassert>
#include "MSXConfig.hh"
#include "xmlx.hh"
#include "Config.hh"
#include "FileContext.hh"
#include "File.hh"

namespace openmsx {

MSXConfig::MSXConfig()
{
}

MSXConfig::~MSXConfig()
{
	for (Configs::const_iterator it = configs.begin();
	     it != configs.end(); ++it) {
		delete *it;
	}
}

void MSXConfig::loadConfig(const XMLElement& config, const FileContext& context)
	throw(ConfigException)
{
	// TODO check duplicate id
	configs.push_back(new Config(config, context));
}

void MSXConfig::handleDoc(const XMLDocument& doc, FileContext& context)
	throw(ConfigException)
{
	const XMLElement::Children& children = doc.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if (((*it)->getName() == "config") || ((*it)->getName() == "device")) {
			string id = (*it)->getAttribute("id");
			if (id == "") {
				throw ConfigException(
					"<config> or <device> is missing 'id'");
			}
			if (hasConfigWithId(id)) {
				throw ConfigException(
				    "<config> or <device> with duplicate 'id':" + id);
			}
			loadConfig(**it, context);
		}
	}
}

bool MSXConfig::hasConfigWithId(const string& id)
{
	return findConfigById(id);
}

Config* MSXConfig::getConfigById(const string& id)
	throw(ConfigException)
{
	Config* result = findConfigById(id);
	if (!result) {
		throw ConfigException("<config> or <device> with id:" + id +
		                      " not found");
	}
	return result;
}

Config* MSXConfig::findConfigById(const string& id)
{
	for (Configs::const_iterator it = configs.begin();
	     it != configs.end(); ++it) {
		if ((*it)->getId() == id) {
			return *it;
		}
	}
	return NULL;
}

} // namespace openmsx
