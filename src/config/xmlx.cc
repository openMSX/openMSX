// $Id$

#include "xmlx.hh"

namespace openmsx {

static string EMPTY("");

XMLException::XMLException(const string& msg)
	: MSXException(msg)
{
}


XMLElement::XMLElement()
{
}

XMLElement::XMLElement(xmlNodePtr node)
{
	init(node);
}

void XMLElement::init(xmlNodePtr node)
{
	name = (const char*)node->name;
	for (xmlNodePtr x = node->children; x != NULL ; x = x->next) {
		switch (x->type) {
		case XML_TEXT_NODE:
			pcdata += (const char*)x->content;
			break;
		case XML_ELEMENT_NODE:
			children.push_back(new XMLElement(x));
			break;
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
			attributes[name] = value;
			break;
		}
		default:
			// ignore
			break;
		}
	}
}

XMLElement::~XMLElement()
{
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		delete *it;
	}
}

const string& XMLElement::getAttribute(const string& attName) const
{
	Attributes::const_iterator it = attributes.find(attName);
	if (it != attributes.end()) {
		return it->second;
	} else {
		return EMPTY;
	}
}

const string& XMLElement::getElementPcdata(const string& childName) const
{
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->name == childName) {
			return (*it)->pcdata;
		}
	}
	return EMPTY;
}


XMLDocument::XMLDocument(const string& filename) throw(XMLException)
{
	xmlDocPtr doc = xmlParseFile(filename.c_str());
	handleDoc(doc);
}

XMLDocument::XMLDocument(const ostringstream& stream) throw(XMLException)
{
	xmlDocPtr doc = xmlParseMemory(stream.str().c_str(), stream.str().length());
	handleDoc(doc);
}

void XMLDocument::handleDoc(xmlDocPtr doc)
{
	if (!doc) {
		throw XMLException("Document parsing failed");
	}
	if (!doc->children || !doc->children->name) {
		xmlFreeDoc(doc);
		throw XMLException("Document doesn't contain mandatory root Element");
	}
	init(xmlDocGetRootElement(doc));
	xmlFreeDoc(doc);
}


const string& XMLEscape(string& str)
{
	xmlChar* buffer = xmlEncodeEntitiesReentrant(NULL, (const xmlChar*)str.c_str());
	str = (const char*)buffer;
	// buffer is allocated in C code, soo we free it the C-way:
	if (buffer != NULL) {
		free(buffer);
	}
	return str;
}

} // namespace openmsx
