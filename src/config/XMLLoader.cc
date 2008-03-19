// $Id$

#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include <libxml/xmlversion.h>

using std::auto_ptr;
using std::string;

namespace openmsx {

auto_ptr<XMLElement> XMLLoader::loadXML(const string& filename,
                                        const string& systemID)
{
#if LIBXML_VERSION < 20600
	xmlDocPtr doc = xmlParseFile(filename.c_str());
#else
	xmlDocPtr doc = xmlReadFile(filename.c_str(), NULL, 0);
#endif
	if (!doc) {
		throw XMLException(filename + ": Document parsing failed");
	}
	if (!doc->children || !doc->children->name) {
		xmlFreeDoc(doc);
		throw XMLException(filename +
			": Document doesn't contain mandatory root Element");
	}
	xmlDtdPtr intSubset = xmlGetIntSubset(doc);
	if (!intSubset) {
		throw XMLException(filename + ": Missing systemID.\n"
			"You're probably using an old incompatible file format.");
	}
	string actualID = reinterpret_cast<const char*>(intSubset->SystemID);
	if (actualID != systemID) {
		throw XMLException(filename + ": systemID doesn't match "
			"(expected " + systemID + ", got " + actualID + ")\n"
			"You're probably using an old incompatible file format.");
	}

	auto_ptr<XMLElement> result(new XMLElement(""));
	init(*result, xmlDocGetRootElement(doc));
	xmlFreeDoc(doc);
	//xmlCleanupParser(); // connecting from openmsx-debugger breaks in
	                      //  windows when this is enabled
	return result;
}

void XMLLoader::init(XMLElement& elem, xmlNodePtr node)
{
	elem.setName(reinterpret_cast<const char*>(node->name));
	for (xmlNodePtr x = node->children; x != NULL ; x = x->next) {
		switch (x->type) {
		case XML_TEXT_NODE:
			elem.setData(elem.getData() +
			             reinterpret_cast<const char*>(x->content));
			break;
		case XML_ELEMENT_NODE: {
			std::auto_ptr<XMLElement> child(new XMLElement(""));
			init(*child, x);
			elem.addChild(child);
			break;
		}
		default:
			// ignore
			break;
		}
	}
	for (xmlAttrPtr x = node->properties; x != NULL ; x = x->next) {
		switch (x->type) {
		case XML_ATTRIBUTE_NODE: {
			string name  = reinterpret_cast<const char*>(x->name);
			string value = reinterpret_cast<const char*>(x->children->content);
			elem.addAttribute(name, value);
			break;
		}
		default:
			// ignore
			break;
		}
	}
}

} // namespace openmsx
