// $Id$

#include "MSXConfig.hh"
#include "FileContext.hh"

namespace openmsx {

void MSXConfig::loadConfig(const FileContext& context, auto_ptr<XMLElement> elem)
{
	elem->setFileContext(auto_ptr<FileContext>(context.clone()));
	addChild(elem);
}

void MSXConfig::handleDoc(const XMLDocument& doc, FileContext& context)
{
	const XMLElement::Children& children = doc.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		loadConfig(context, auto_ptr<XMLElement>(new XMLElement(**it)));
	}
}

} // namespace openmsx
