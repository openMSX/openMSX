// $Id$

#include <cassert>
#include "StringOp.hh"
#include "FileContext.hh"
#include "xmlx.hh"

namespace openmsx {

static const string EMPTY;


// class XMLException

XMLException::XMLException(const string& msg)
	: MSXException(msg)
{
}


// class XMLElement

XMLElement::XMLElement()
	: parent(NULL)
{
}

XMLElement::XMLElement(xmlNodePtr node)
	: parent(NULL)
{
	init(node);
}

XMLElement::XMLElement(const string& name_, const string& data_)
	: name(name_), data(data_), parent(NULL)
{
}

XMLElement::XMLElement(const XMLElement& element)
	: parent(NULL)
{
	*this = element;
}

void XMLElement::init(xmlNodePtr node)
{
	name = (const char*)node->name;
	for (xmlNodePtr x = node->children; x != NULL ; x = x->next) {
		switch (x->type) {
		case XML_TEXT_NODE:
			data += (const char*)x->content;
			break;
		case XML_ELEMENT_NODE:
			addChild(auto_ptr<XMLElement>(new XMLElement(x)));
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

XMLElement* XMLElement::getParent()
{
	return parent;
}

const XMLElement* XMLElement::getParent() const
{
	return parent;
}

void XMLElement::addChild(auto_ptr<XMLElement> child)
{
	assert(child.get());
	assert(!child->getParent());
	child->parent = this;
	children.push_back(child.release());
}

void XMLElement::addAttribute(const string& name, const string& value)
{
	assert(attributes.find(name) == attributes.end());
	attributes[name] = value;
}

void XMLElement::getChildren(const string& name, Children& result) const
{
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == name) {
			result.push_back(*it);
		}
	}
}

const XMLElement* XMLElement::getChild(const string& name) const
{
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}

const string& XMLElement::getChildData(const string& name) const
{
	const XMLElement* child = getChild(name);
	return child ? child->getData() : EMPTY;
}

string XMLElement::getChildData(const string& name,
                                const string& defaultValue) const
{
	const XMLElement* child = getChild(name);
	return child ? child->getData() : defaultValue;
}

bool XMLElement::getChildDataAsBool(const string& name, bool defaultValue) const
{
	const XMLElement* child = getChild(name);
	return child ? StringOp::stringToBool(child->getData()) : defaultValue;
}

int XMLElement::getChildDataAsInt(const string& name, int defaultValue) const
{
	const XMLElement* child = getChild(name);
	return child ? StringOp::stringToInt(child->getData()) : defaultValue;
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

void XMLElement::setFileContext(auto_ptr<FileContext> context_)
{
	context = context_;
}

FileContext& XMLElement::getFileContext() const
{
	return context.get() ? *context.get() : getParent()->getFileContext();
}

const XMLElement& XMLElement::operator=(const XMLElement& element)
{
	if (&element == this) {
		// assign to itself
		return *this;
	}
	name = element.name;
	data = element.data;
	attributes = element.attributes;
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		delete *it;
	}
	children.clear();
	for (Children::const_iterator it = element.children.begin();
	     it != element.children.end(); ++it) {
		addChild(auto_ptr<XMLElement>(new XMLElement(**it)));
	}
	return *this;
}


// class XMLDocument

XMLDocument::XMLDocument(const string& filename)
{
	xmlDocPtr doc = xmlParseFile(filename.c_str());
	handleDoc(doc);
}

XMLDocument::XMLDocument(const ostringstream& stream)
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
