// $Id$

#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include <cassert>
#include <cstring>
#include <libxml/parser.h>

using std::auto_ptr;
using std::string;

namespace openmsx {
namespace XMLLoader {

struct XMLLoaderHelper
{
	XMLLoaderHelper()
		: current(NULL)
	{
	}

	std::auto_ptr<XMLElement> root;
	XMLElement* current;
	std::string data;
	std::string systemID;
};

static void cbStartElement(XMLLoaderHelper* helper, const xmlChar* name,
                           const xmlChar** attrs)
{
	std::auto_ptr<XMLElement> newElem(
		new XMLElement(reinterpret_cast<const char*>(name)));

	if (attrs) {
		for (/**/; *attrs; attrs += 2) {
			newElem->addAttribute(reinterpret_cast<const char*>(attrs[0]),
			                      reinterpret_cast<const char*>(attrs[1]));
		}
	}

	XMLElement* newElem2 = newElem.get();
	if (helper->current) {
		helper->current->addChild(newElem);
	} else {
		helper->root = newElem;
	}
	helper->current = newElem2;

	helper->data.clear();
}

static void cbEndElement(XMLLoaderHelper* helper, const xmlChar* name)
{
	assert(helper->current);
	assert(reinterpret_cast<const char*>(name) == helper->current->getName());
	(void)name;

	helper->current->setData(helper->data);
	helper->current = helper->current->getParent();
	helper->data.clear();
}

static void cbCharacters(XMLLoaderHelper* helper, const xmlChar* chars, int len)
{
	assert(helper->current);
	helper->data.append(reinterpret_cast<const char*>(chars), len);
}

static void cbInternalSubset(XMLLoaderHelper* helper, const xmlChar* /*name*/,
                             const xmlChar* /*extID*/, const xmlChar* systemID)
{
	helper->systemID = reinterpret_cast<const char*>(systemID);
}

auto_ptr<XMLElement> load(const string& filename, const string& systemID)
{
	xmlSAXHandler handler;
	memset(&handler, 0, sizeof(handler));
	handler.startElement  = (startElementSAXFunc)   cbStartElement;
	handler.endElement    = (endElementSAXFunc)     cbEndElement;
	handler.characters    = (charactersSAXFunc)     cbCharacters;
	handler.internalSubset = (internalSubsetSAXFunc)cbInternalSubset;

	XMLLoaderHelper helper;
	if (xmlSAXUserParseFile(&handler, &helper, filename.c_str())) {
		throw XMLException(filename + ": Document parsing failed");
	}

	if (!helper.root.get()) {
		throw XMLException(filename +
			": Document doesn't contain mandatory root Element");
	}
	if (helper.systemID.empty()) {
		throw XMLException(filename + ": Missing systemID.\n"
			"You're probably using an old incompatible file format.");
	}
	if (helper.systemID != systemID) {
		throw XMLException(filename + ": systemID doesn't match "
			"(expected " + systemID + ", got " + helper.systemID + ")\n"
			"You're probably using an old incompatible file format.");
	}

	return helper.root;
}

} // namespace XMLLoader
} // namespace openmsx
