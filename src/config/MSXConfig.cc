// $Id$

#include "MSXConfig.hh"
#include "FileContext.hh"

namespace openmsx {

MSXConfig::MSXConfig(const string& name)
	: XMLElement(name)
{
	setFileContext(auto_ptr<FileContext>(new SystemFileContext()));
}

void MSXConfig::handleDoc(XMLElement& root, const XMLDocument& doc,
                          FileContext& context)
{
	const XMLElement::Children& children = doc.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		auto_ptr<XMLElement> elem(new XMLElement(**it));
		elem->setFileContext(auto_ptr<FileContext>(context.clone()));
		root.addChild(elem);
	}
}

} // namespace openmsx
