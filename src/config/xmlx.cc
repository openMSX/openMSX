// $Id$

#include <cassert>
#include "xmlx.hh"

namespace openmsx {

static string EMPTY("");


// class XMLException

XMLException::XMLException(const string& msg)
	: MSXException(msg)
{
}


// class XMLElement

XMLElement::XMLElement()
{
}

XMLElement::XMLElement(xmlNodePtr node)
{
	init(node);
}

XMLElement::XMLElement(const string& name_, const string& pcdata_)
	: name(name_), pcdata(pcdata_)
{
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
			addAttribute(name, value);
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

void XMLElement::addChild(XMLElement* child)
{
	assert(child);
	children.push_back(child);
}

void XMLElement::addAttribute(const string& name, const string& value)
{
	assert(attributes.find(name) == attributes.end());
	attributes[name] = value;
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

XMLElement::XMLElement(const XMLElement& element)
{
	*this = element;
}

const XMLElement& XMLElement::operator=(const XMLElement& element)
{
	if (&element == this) {
		// assign to itself
		return *this;
	}
	name = element.name;
	pcdata = element.pcdata;
	attributes = element.attributes;
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		delete *it;
	}
	children.clear();
	for (Children::const_iterator it = element.children.begin();
	     it != element.children.end(); ++it) {
		children.push_back(new XMLElement(**it));
	}
	return *this;
}


// class XMLDocument

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

XMLDocument::XMLDocument(const XMLDocument& document)
{
	*this = document;
}

const XMLDocument& XMLDocument::operator=(const XMLDocument& document)
{
	this->XMLElement::operator=(document);
	return *this;
}



string XMLEscape(const string& str)
{
	xmlChar* buffer = xmlEncodeEntitiesReentrant(NULL, (const xmlChar*)str.c_str());
	string result = (const char*)buffer;
	// buffer is allocated in C code, soo we free it the C-way:
	if (buffer != NULL) {
		free(buffer);
	}
	return result;
}

} // namespace openmsx
