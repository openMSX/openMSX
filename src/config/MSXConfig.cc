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
{
	// TODO check duplicate id
	configs.push_back(new Config(config, context));
}

void MSXConfig::handleDoc(const XMLDocument& doc, FileContext& context)
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
			if (findConfigById(id)) {
				throw ConfigException(
				    "<config> or <device> with duplicate 'id':" + id);
			}
			loadConfig(**it, context);
		}
	}
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
