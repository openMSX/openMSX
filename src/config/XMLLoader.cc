// $Id$

#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "StringOp.hh"

using std::auto_ptr;
using std::string;

namespace openmsx {

// class XMLException

XMLException::XMLException(const string& msg)
	: MSXException(msg)
{
}

// class XMLLoader

auto_ptr<XMLElement> XMLLoader::loadXML(const string& filename,
                                        const string& systemID,
                                        IdMap* idMap)
{
	xmlDocPtr doc = xmlParseFile(filename.c_str());
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
	string actualID = (const char*)intSubset->SystemID;
	if (actualID != systemID) {
		throw XMLException(filename + ": systemID doesn't match "
			"(expected " + systemID + ", got " + actualID + ")\n"
			"You're probably using an old incompatible file format.");
	}

	auto_ptr<XMLElement> result(new XMLElement(""));
	init(*result, xmlDocGetRootElement(doc), idMap);
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return result;
}

void XMLLoader::init(XMLElement& elem, xmlNodePtr node, IdMap* idMap)
{
	elem.setName((const char*)node->name);
	for (xmlNodePtr x = node->children; x != NULL ; x = x->next) {
		switch (x->type) {
		case XML_TEXT_NODE:
			elem.setData(elem.getData() + (const char*)x->content);
			break;
		case XML_ELEMENT_NODE: {
			std::auto_ptr<XMLElement> child(new XMLElement(""));
			init(*child, x, idMap);
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
			string name  = (const char*)x->name;
			string value = (const char*)x->children->content;
			if (idMap && name == "id") {
				value = makeUnique(value, *idMap);
			}
			elem.addAttribute(name, value);
			break;
		}
		default:
			// ignore
			break;
		}
	}
}

string XMLLoader::makeUnique(const string& str, IdMap& idMap)
{
	unsigned num = ++idMap[str];
	if (num == 1) {
		return str;
	} else {
		return str + " (" + StringOp::toString(num) + ')';
	}
}

} // namespace openmsx
