// $Id$

#include <cassert>
#include <algorithm>
#include <libxml/uri.h>
#include "StringOp.hh"
#include "FileContext.hh"
#include "ConfigException.hh"
#include "xmlx.hh"

using std::remove;

namespace openmsx {

// class XMLException

XMLException::XMLException(const string& msg)
	: MSXException(msg)
{
}


// class XMLElement

map<string, unsigned> XMLElement::idMap;

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
			if (name == "id") {
				value = makeUnique(value);
			}
			addAttribute(name, value);
			break;
		}
		default:
			// ignore
			break;
		}
	}
}

string XMLElement::makeUnique(const string& str)
{
	unsigned num = ++idMap[str];
	if (num == 1) {
		return str;
	} else {
		return str + " (" + StringOp::toString(num) + ')';
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

void XMLElement::deleteChild(const XMLElement& child)
{
	assert(find(children.begin(), children.end(), &child) != children.end());
	children.erase(remove(children.begin(), children.end(), &child),
	               children.end());
	delete &child;
}

void XMLElement::addAttribute(const string& name, const string& value)
{
	assert(attributes.find(name) == attributes.end());
	attributes[name] = value;
}

bool XMLElement::getDataAsBool() const
{
	return StringOp::stringToBool(getData());
}

int XMLElement::getDataAsInt() const
{
	return StringOp::stringToInt(getData());
}

double XMLElement::getDataAsDouble() const
{
	return StringOp::stringToDouble(getData());
}

void XMLElement::setData(const string& data_)
{
	assert(children.empty()); // no mixed-content elements
	data = data_;
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

XMLElement* XMLElement::findChild(const string& name)
{
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}

const XMLElement* XMLElement::findChild(const string& name) const
{
	return const_cast<XMLElement*>(this)->findChild(name);
}

XMLElement& XMLElement::getChild(const string& name)
{
	XMLElement* elem = findChild(name);
	if (!elem) {
		throw ConfigException("Missing tag \"" + name + "\".");
	}
	return *elem;
}

const XMLElement& XMLElement::getChild(const string& name) const
{
	return const_cast<XMLElement*>(this)->getChild(name);
}

XMLElement& XMLElement::getCreateChild(const string& name,
                                       const string& defaultValue)
{
	XMLElement* result = findChild(name);
	if (!result) {
		result = new XMLElement(name, defaultValue);
		addChild(auto_ptr<XMLElement>(result));
	}
	return *result;
}

XMLElement& XMLElement::getCreateChildWithAttribute(
	const string& name, const string& attName,
	const string& attValue, const string& defaultValue)
{
	Children children;
	getChildren(name, children);
	for (Children::iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getAttribute(attName) == attValue) {
			return **it;
		}
	}
	XMLElement* result = new XMLElement(name, defaultValue);
	result->addAttribute(attName, attValue);
	addChild(auto_ptr<XMLElement>(result));
	return *result;
}

const string& XMLElement::getChildData(const string& name) const
{
	const XMLElement& child = getChild(name);
	return child.getData();
}

string XMLElement::getChildData(const string& name,
                                const string& defaultValue) const
{
	const XMLElement* child = findChild(name);
	return child ? child->getData() : defaultValue;
}

bool XMLElement::getChildDataAsBool(const string& name, bool defaultValue) const
{
	const XMLElement* child = findChild(name);
	return child ? StringOp::stringToBool(child->getData()) : defaultValue;
}

int XMLElement::getChildDataAsInt(const string& name, int defaultValue) const
{
	const XMLElement* child = findChild(name);
	return child ? StringOp::stringToInt(child->getData()) : defaultValue;
}

bool XMLElement::hasAttribute(const string& name) const
{
	return attributes.find(name) != attributes.end();
}

const XMLElement::Attributes& XMLElement::getAttributes() const
{
	return attributes;
}

const string& XMLElement::getAttribute(const string& attName) const
{
	Attributes::const_iterator it = attributes.find(attName);
	if (it == attributes.end()) {
		throw ConfigException("Missing attribute \"" + attName + "\".");
	}
	return it->second;
}

const string XMLElement::getAttribute(const string& attName,
	                              const string defaultValue) const
{
	Attributes::const_iterator it = attributes.find(attName);
	return (it == attributes.end()) ? defaultValue : it->second;
}

bool XMLElement::getAttributeAsBool(const string& attName,
                                    bool defaultValue) const
{
	Attributes::const_iterator it = attributes.find(attName);
	return (it == attributes.end()) ? defaultValue
	                                : StringOp::stringToBool(it->second);
}

int XMLElement::getAttributeAsInt(const string& attName,
                                  int defaultValue) const
{
	Attributes::const_iterator it = attributes.find(attName);
	return (it == attributes.end()) ? defaultValue
	                                : StringOp::stringToInt(it->second);
}

const string& XMLElement::getId() const
{
	const XMLElement* elem = this;
	while (elem) {
		Attributes::const_iterator it = elem->attributes.find("id");
		if (it != elem->attributes.end()) {
			return it->second;
		}
		elem = elem->getParent();
	}
	throw ConfigException("Missing attribute \"id\".");
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

string XMLElement::dump() const
{
	string result;
	dump(result, 0);
	return result;
}

void XMLElement::dump(string& result, unsigned indentNum) const
{
	string indent(indentNum, ' ');
	result += indent + '<' + getName();
	for (Attributes::const_iterator it = attributes.begin();
	     it != attributes.end(); ++it) {
		result += ' ' + it->first +
		          "=\"" + XMLEscape(it->second) + '"';
	}
	if (children.empty()) {
		if (data.empty()) {
			result += "/>\n";
		} else {
			result += '>' + XMLEscape(data) + "</" +
			          getName() + ">\n";
		}
	} else {
		result += ">\n";
		for (Children::const_iterator it = children.begin();
		     it != children.end(); ++it) {
			(*it)->dump(result, indentNum + 2);
		}
		result += indent + "</" + getName() + ">\n";
	}
}

string XMLElement::XMLEscape(const string& str)
{
	xmlChar* buffer = xmlEncodeEntitiesReentrant(NULL, (const xmlChar*)str.c_str());
	string result = (const char*)buffer;
	// buffer is allocated in C code, soo we free it the C-way:
	if (buffer != NULL) {
		xmlFree(buffer);
	}
	return result;
}


// class XMLDocument

XMLDocument::XMLDocument(const string& filename, const string& systemID)
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
		throw XMLException(filename + ": Missing systemID");
	}
	string actualID = (const char*)intSubset->SystemID;
	if (actualID != systemID) {
		throw XMLException(filename + ": systemID doesn't match (" +
			systemID + ", " + actualID + ")\n"
			"You're probably using an old incompatible file format.\n"
			"Try upgrading your configuration files.");
	}
	init(xmlDocGetRootElement(doc));
	xmlFreeDoc(doc);
}

} // namespace openmsx
