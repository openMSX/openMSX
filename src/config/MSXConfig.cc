// $Id$

#include <cassert>
#include "MSXConfig.hh"
#include "xmlx.hh"
#include "FileContext.hh"
#include "File.hh"
#include "ConfigException.hh"

namespace openmsx {

MSXConfig::MSXConfig()
	: root("hardware")
{
}

MSXConfig::~MSXConfig()
{
}

void MSXConfig::loadConfig(const FileContext& context, auto_ptr<XMLElement> elem)
{
	elem->setFileContext(auto_ptr<FileContext>(context.clone()));
	root.addChild(elem);
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
			auto_ptr<XMLElement> copy(new XMLElement(**it));
			loadConfig(context, copy);
		}
	}
}

const XMLElement* MSXConfig::findConfigById(const string& id)
{
	const XMLElement::Children& children = root.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getAttribute("id") == id) {
			return *it;
		}
	}
	return NULL;
}

const XMLElement& MSXConfig::getRoot()
{
	return root;
}

} // namespace openmsx
