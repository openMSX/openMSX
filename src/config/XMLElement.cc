// $Id$

#include "XMLElement.hh"
#include "XMLElementListener.hh"
#include "StringOp.hh"
#include "FileContext.hh"
#include "ConfigException.hh"
#include <libxml/uri.h>
#include <cassert>
#include <algorithm>


using std::auto_ptr;
using std::remove;
using std::string;

namespace openmsx {

XMLElement::XMLElement(const string& name_, const string& data_)
	: name(name_), data(data_), parent(NULL)
{
}

XMLElement::XMLElement(const XMLElement& element)
	: parent(NULL)
{
	*this = element;
}

XMLElement::~XMLElement()
{
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		delete *it;
	}
	assert(listeners.empty());
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
	XMLElement* child2 = child.release();
	children.push_back(child2);
	
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->childAdded(*this, *child2);
	}
}

auto_ptr<XMLElement> XMLElement::removeChild(const XMLElement& child)
{
	assert(std::count(children.begin(), children.end(), &child) == 1);
	children.erase(std::find(children.begin(), children.end(), &child));
	XMLElement& child2 = const_cast<XMLElement&>(child);
	child2.parent = NULL;
	return auto_ptr<XMLElement>(&child2);
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

void XMLElement::setName(const std::string& name_)
{
	name = name_;
}

void XMLElement::setData(const string& data_)
{
	//assert(children.empty()); // no mixed-content elements
	data = data_;
	for (Listeners::const_iterator it = listeners.begin();
	     it != listeners.end(); ++it) {
		(*it)->updateData(*this);
	}
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

struct ShallowEqualTo {
	ShallowEqualTo(const XMLElement& rhs_) : rhs(rhs_) {}
	bool operator()(const XMLElement* lhs) const {
		return lhs->isShallowEqual(rhs);
	}
	const XMLElement& rhs;
};

void XMLElement::merge(const XMLElement& source)
{
	assert(isShallowEqual(source));
	Children srcChildrenCopy(children.begin(), children.end());
	const Children& sourceChildren = source.getChildren();
	for (Children::const_iterator it = sourceChildren.begin();
	     it != sourceChildren.end(); ++it) {
		const XMLElement& srcChild = **it;
		Children::iterator it = std::find_if(srcChildrenCopy.begin(),
			srcChildrenCopy.end(), ShallowEqualTo(srcChild));
		if (it != srcChildrenCopy.end()) {
			(*it)->merge(srcChild);
			srcChildrenCopy.erase(it); // don't merge to same child twice
		} else {
			addChild(auto_ptr<XMLElement>(new XMLElement(srcChild)));
		}
	}
	setData(source.getData());
}

bool XMLElement::isShallowEqual(const XMLElement& other) const
{
	return (getName()       == other.getName()) &&
	       (getAttributes() == other.getAttributes());
}

void XMLElement::addListener(XMLElementListener& listener)
{
	listeners.push_back(&listener);
}

void XMLElement::removeListener(XMLElementListener& listener)
{
	assert(count(listeners.begin(), listeners.end(), &listener) == 1);
	listeners.erase(find(listeners.begin(), listeners.end(), &listener));
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

} // namespace openmsx
