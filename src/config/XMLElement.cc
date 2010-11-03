// $Id$

#include "XMLElement.hh"
#include "StringOp.hh"
#include "FileContext.hh"
#include "ConfigException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include <libxml/uri.h>
#include <cassert>
#include <algorithm>

using std::auto_ptr;
using std::string;

namespace openmsx {

XMLElement::XMLElement(const string& name_)
	: name(name_), parent(NULL)
{
}

XMLElement::XMLElement(const char* name_)
	: name(name_), parent(NULL)
{
}

XMLElement::XMLElement(const string& name_, const string& data_)
	: name(name_), data(data_), parent(NULL)
{
}

XMLElement::XMLElement(const char* name_, const char* data_)
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
	removeAllChildren();
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
	// Mixed-content elements are not supported by this class. In the past
	// we had a 'assert(data.empty())' here to enforce this, though that
	// assert triggered when you started openMSX without a user (but with
	// a system) settings.xml file (the deeper reason is a harmless comment
	// in the system version of this file).
	// When you add child nodes to a node with data, that data will be
	// ignored when this node is later written to disk. In the case of
	// settings.xml this behaviour is fine.
	assert(child.get());
	assert(!child->getParent());
	child->parent = this;
	XMLElement* child2 = child.release();
	children.push_back(child2);
}

auto_ptr<XMLElement> XMLElement::removeChild(const XMLElement& child)
{
	assert(std::count(children.begin(), children.end(), &child) == 1);
	children.erase(std::find(children.begin(), children.end(), &child));
	XMLElement& child2 = const_cast<XMLElement&>(child);
	child2.parent = NULL;
	return auto_ptr<XMLElement>(&child2);
}

XMLElement::Attributes::iterator XMLElement::findAttribute(const char* name_)
{
	string name(name_);
	for (Attributes::iterator it = attributes.begin();
	     it != attributes.end(); ++it) {
		if (it->first == name) {
			return it;
		}
	}
	return attributes.end();
}
XMLElement::Attributes::const_iterator XMLElement::findAttribute(const char* name_) const
{
	string name(name_);
	for (Attributes::const_iterator it = attributes.begin();
	     it != attributes.end(); ++it) {
		if (it->first == name) {
			return it;
		}
	}
	return attributes.end();
}

void XMLElement::addAttribute(const char* name, const string& value)
{
	assert(findAttribute(name) == attributes.end());
	attributes.push_back(make_pair(string(name), value));
}

void XMLElement::setAttribute(const char* name, const string& value)
{
	Attributes::iterator it = findAttribute(name);
	if (it != attributes.end()) {
		it->second = value;
	}
	attributes.push_back(make_pair(name, value));
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

void XMLElement::setName(const string& name_)
{
	name = name_;
}

void XMLElement::clearName()
{
	name.clear();
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
void XMLElement::getChildren(const char* name, Children& result) const
{
	getChildren(string(name), result);
}

XMLElement* XMLElement::findChild(const char* name_)
{
	string name(name_);
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}
const XMLElement* XMLElement::findChild(const char* name) const
{
	return const_cast<XMLElement*>(this)->findChild(name);
}

const XMLElement* XMLElement::findNextChild(const char* name,
	                                    unsigned& fromIndex) const
{
	unsigned numChildren = unsigned(children.size());
	for (unsigned i = fromIndex; i != numChildren; ++i) {
		if (children[i]->getName() == name) {
			fromIndex = i + 1;
			return children[i];
		}
	}
	for (unsigned i = 0; i < fromIndex; ++i) {
		if (children[i]->getName() == name) {
			fromIndex = i + 1;
			return children[i];
		}
	}
	return NULL;
}

XMLElement* XMLElement::findChildWithAttribute(const char* name,
	const char* attName, const string& attValue)
{
	Children children;
	getChildren(name, children);
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getAttribute(attName) == attValue) {
			return *it;
		}
	}
	return NULL;
}

const XMLElement* XMLElement::findChildWithAttribute(const char* name,
	const char* attName, const string& attValue) const
{
	return const_cast<XMLElement*>(this)->findChildWithAttribute(
		name, attName, attValue);
}

XMLElement& XMLElement::getChild(const char* name)
{
	XMLElement* elem = findChild(name);
	if (!elem) {
		throw ConfigException(StringOp::Builder() <<
			"Missing tag \"" << name << "\".");
	}
	return *elem;
}
const XMLElement& XMLElement::getChild(const char* name) const
{
	return const_cast<XMLElement*>(this)->getChild(name);
}

XMLElement& XMLElement::getCreateChild(const char* name,
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
	const char* name, const char* attName,
	const string& attValue)
{
	XMLElement* result = findChildWithAttribute(name, attName, attValue);
	if (!result) {
		result = new XMLElement(name);
		result->addAttribute(attName, attValue);
		addChild(auto_ptr<XMLElement>(result));
	}
	return *result;
}

const string& XMLElement::getChildData(const char* name) const
{
	const XMLElement& child = getChild(name);
	return child.getData();
}

string XMLElement::getChildData(const char* name,
                                const char* defaultValue) const
{
	const XMLElement* child = findChild(name);
	return child ? child->getData() : defaultValue;
}

bool XMLElement::getChildDataAsBool(const char* name, bool defaultValue) const
{
	const XMLElement* child = findChild(name);
	return child ? StringOp::stringToBool(child->getData()) : defaultValue;
}

int XMLElement::getChildDataAsInt(const char* name, int defaultValue) const
{
	const XMLElement* child = findChild(name);
	return child ? StringOp::stringToInt(child->getData()) : defaultValue;
}

void XMLElement::setChildData(const char* name, const string& value)
{
	XMLElement* child = findChild(name);
	if (child) {
		child->setData(value);
	} else {
		addChild(auto_ptr<XMLElement>(new XMLElement(name, value)));
	}
}

void XMLElement::removeAllChildren()
{
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		delete *it;
	}
	children.clear();
}

bool XMLElement::hasAttribute(const char* name) const
{
	return findAttribute(name) != attributes.end();
}

const string& XMLElement::getAttribute(const char* attName) const
{
	Attributes::const_iterator it = findAttribute(attName);
	if (it == attributes.end()) {
		throw ConfigException("Missing attribute \"" +
		                      string(attName) + "\".");
	}
	return it->second;
}

const string XMLElement::getAttribute(const char* attName,
	                              const char* defaultValue) const
{
	Attributes::const_iterator it = findAttribute(attName);
	return (it == attributes.end()) ? defaultValue : it->second;
}

bool XMLElement::getAttributeAsBool(const char* attName,
                                    bool defaultValue) const
{
	Attributes::const_iterator it = findAttribute(attName);
	return (it == attributes.end()) ? defaultValue
	                                : StringOp::stringToBool(it->second);
}

int XMLElement::getAttributeAsInt(const char* attName,
                                  int defaultValue) const
{
	Attributes::const_iterator it = findAttribute(attName);
	return (it == attributes.end()) ? defaultValue
	                                : StringOp::stringToInt(it->second);
}

bool XMLElement::findAttributeInt(const char* attName,
                                  unsigned& result) const
{
	Attributes::const_iterator it = findAttribute(attName);
	if (it != attributes.end()) {
		result = StringOp::stringToInt(it->second);
		return true;
	} else {
		return false;
	}
}

const string& XMLElement::getId() const
{
	const XMLElement* elem = this;
	while (elem) {
		Attributes::const_iterator it = elem->findAttribute("id");
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

	removeAllChildren();
	for (Children::const_iterator it = element.children.begin();
	     it != element.children.end(); ++it) {
		addChild(auto_ptr<XMLElement>(new XMLElement(**it)));
	}
	return *this;
}

string XMLElement::dump() const
{
	StringOp::Builder result;
	dump(result, 0);
	return result;
}

void XMLElement::dump(StringOp::Builder& result, unsigned indentNum) const
{
	string indent(indentNum, ' ');
	result << indent << '<' << getName();
	for (Attributes::const_iterator it = attributes.begin();
	     it != attributes.end(); ++it) {
		result << ' ' << it->first
		       << "=\"" << XMLEscape(it->second) << '"';
	}
	if (children.empty()) {
		if (data.empty()) {
			result << "/>\n";
		} else {
			result << '>' << XMLEscape(data) << "</"
			       << getName() << ">\n";
		}
	} else {
		result << ">\n";
		for (Children::const_iterator it = children.begin();
		     it != children.end(); ++it) {
			(*it)->dump(result, indentNum + 2);
		}
		result << indent << "</" << getName() << ">\n";
	}
}

string XMLElement::XMLEscape(const string& str)
{
	xmlChar* buffer = xmlEncodeEntitiesReentrant(
		NULL, reinterpret_cast<const xmlChar*>(str.c_str()));
	string result = reinterpret_cast<const char*>(buffer);
	xmlFree(buffer);
	return result;
}

template<typename Archive>
void XMLElement::serialize(Archive& ar, unsigned /*version*/)
{
	// note1: filecontext is not (yet?) serialized
	//
	// note2: In the past attributes were stored in a map instead of a
	//        vector. To keep backwards compatible with the serialized
	//        format, we still convert attributes to this format.
	typedef std::map<string, string> AttributesMap;

	if (!ar.isLoader()) {
		AttributesMap tmpAtt(attributes.begin(), attributes.end());
		ar.serialize("attributes", tmpAtt);
		ar.serialize("children", getChildren());
	} else {
		AttributesMap tmpAtt;
		ar.serialize("attributes", tmpAtt);
		for (AttributesMap::const_iterator it = tmpAtt.begin();
		     it != tmpAtt.end(); ++it) {
			// TODO "string -> char* -> string" conversion can
			//       be optimized
			addAttribute(it->first.c_str(), it->second);
		}

		XMLElement::Children tmp;
		ar.serialize("children", tmp);
		for (XMLElement::Children::const_iterator it = tmp.begin();
		     it != tmp.end(); ++it) {
			addChild(auto_ptr<XMLElement>(*it));
		}
	}
	ar.serialize("context", context);
}
INSTANTIATE_SERIALIZE_METHODS(XMLElement);

} // namespace openmsx
