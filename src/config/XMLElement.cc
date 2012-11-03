// $Id$

#include "XMLElement.hh"
#include "StringOp.hh"
#include "FileContext.hh" // for bwcompat
#include "ConfigException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include <libxml/uri.h>
#include <cassert>
#include <algorithm>

using std::unique_ptr;
using std::string;

namespace openmsx {

XMLElement::XMLElement(string_ref name_)
	: name(name_.data(), name_.size())
{
}

XMLElement::XMLElement(string_ref name_, string_ref data_)
	: name(name_.data(), name_.size())
	, data(data_.data(), data_.size())
{
}

XMLElement::XMLElement(const XMLElement& element)
{
	*this = element;
}

XMLElement::~XMLElement()
{
	removeAllChildren();
}

void XMLElement::addChild(unique_ptr<XMLElement> child)
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
	XMLElement* child2 = child.release();
	children.push_back(child2);
}

unique_ptr<XMLElement> XMLElement::removeChild(const XMLElement& child)
{
	assert(std::count(children.begin(), children.end(), &child) == 1);
	children.erase(std::find(children.begin(), children.end(), &child));
	XMLElement& child2 = const_cast<XMLElement&>(child);
	return unique_ptr<XMLElement>(&child2);
}

XMLElement::Attributes::iterator XMLElement::findAttribute(string_ref name)
{
	for (Attributes::iterator it = attributes.begin();
	     it != attributes.end(); ++it) {
		if (it->first == name) {
			return it;
		}
	}
	return attributes.end();
}
XMLElement::Attributes::const_iterator XMLElement::findAttribute(string_ref name) const
{
	for (Attributes::const_iterator it = attributes.begin();
	     it != attributes.end(); ++it) {
		if (it->first == name) {
			return it;
		}
	}
	return attributes.end();
}

void XMLElement::addAttribute(string_ref name, string_ref value)
{
	assert(findAttribute(name) == attributes.end());
	attributes.push_back(make_pair(name.str(), value.str()));
}

void XMLElement::setAttribute(string_ref name, string_ref value)
{
	Attributes::iterator it = findAttribute(name);
	if (it != attributes.end()) {
		it->second.assign(value.data(), value.size());
	} else {
		attributes.push_back(make_pair(name.str(), value.str()));
	}
}

void XMLElement::removeAttribute(string_ref name)
{
	Attributes::iterator it = findAttribute(name);
	if (it != attributes.end()) {
		attributes.erase(it);
	}
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

void XMLElement::setName(string_ref name_)
{
	name.assign(name_.data(), name_.size());
}

void XMLElement::clearName()
{
	name.clear();
}

void XMLElement::setData(string_ref data_)
{
	assert(children.empty()); // no mixed-content elements
	data.assign(data_.data(), data_.size());
}

void XMLElement::getChildren(string_ref name, Children& result) const
{
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == name) {
			result.push_back(*it);
		}
	}
}

XMLElement* XMLElement::findChild(string_ref name)
{
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return nullptr;
}
const XMLElement* XMLElement::findChild(string_ref name) const
{
	return const_cast<XMLElement*>(this)->findChild(name);
}

const XMLElement* XMLElement::findNextChild(string_ref name,
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
	return nullptr;
}

XMLElement* XMLElement::findChildWithAttribute(string_ref name,
	string_ref attName, string_ref attValue)
{
	Children children;
	getChildren(name, children);
	for (Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getAttribute(attName) == attValue) {
			return *it;
		}
	}
	return nullptr;
}

const XMLElement* XMLElement::findChildWithAttribute(string_ref name,
	string_ref attName, string_ref attValue) const
{
	return const_cast<XMLElement*>(this)->findChildWithAttribute(
		name, attName, attValue);
}

XMLElement& XMLElement::getChild(string_ref name)
{
	XMLElement* elem = findChild(name);
	if (!elem) {
		throw ConfigException(StringOp::Builder() <<
			"Missing tag \"" << name << "\".");
	}
	return *elem;
}
const XMLElement& XMLElement::getChild(string_ref name) const
{
	return const_cast<XMLElement*>(this)->getChild(name);
}

XMLElement& XMLElement::getCreateChild(string_ref name,
                                       string_ref defaultValue)
{
	XMLElement* result = findChild(name);
	if (!result) {
		result = new XMLElement(name, defaultValue);
		addChild(unique_ptr<XMLElement>(result));
	}
	return *result;
}

XMLElement& XMLElement::getCreateChildWithAttribute(
	string_ref name, string_ref attName,
	string_ref attValue)
{
	XMLElement* result = findChildWithAttribute(name, attName, attValue);
	if (!result) {
		result = new XMLElement(name);
		result->addAttribute(attName, attValue);
		addChild(unique_ptr<XMLElement>(result));
	}
	return *result;
}

const string& XMLElement::getChildData(string_ref name) const
{
	const XMLElement& child = getChild(name);
	return child.getData();
}

string_ref XMLElement::getChildData(string_ref name,
                                    string_ref defaultValue) const
{
	const XMLElement* child = findChild(name);
	return child ? child->getData() : defaultValue;
}

bool XMLElement::getChildDataAsBool(string_ref name, bool defaultValue) const
{
	const XMLElement* child = findChild(name);
	return child ? StringOp::stringToBool(child->getData()) : defaultValue;
}

int XMLElement::getChildDataAsInt(string_ref name, int defaultValue) const
{
	const XMLElement* child = findChild(name);
	return child ? StringOp::stringToInt(child->getData()) : defaultValue;
}

void XMLElement::setChildData(string_ref name, string_ref value)
{
	if (XMLElement* child = findChild(name)) {
		child->setData(value);
	} else {
		addChild(unique_ptr<XMLElement>(new XMLElement(name, value)));
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

bool XMLElement::hasAttribute(string_ref name) const
{
	return findAttribute(name) != attributes.end();
}

const string& XMLElement::getAttribute(string_ref attName) const
{
	Attributes::const_iterator it = findAttribute(attName);
	if (it == attributes.end()) {
		throw ConfigException("Missing attribute \"" +
		                      attName + "\".");
	}
	return it->second;
}

string_ref XMLElement::getAttribute(string_ref attName,
	                            string_ref defaultValue) const
{
	Attributes::const_iterator it = findAttribute(attName);
	return (it == attributes.end()) ? defaultValue : it->second;
}

bool XMLElement::getAttributeAsBool(string_ref attName,
                                    bool defaultValue) const
{
	Attributes::const_iterator it = findAttribute(attName);
	return (it == attributes.end()) ? defaultValue
	                                : StringOp::stringToBool(it->second);
}

int XMLElement::getAttributeAsInt(string_ref attName,
                                  int defaultValue) const
{
	Attributes::const_iterator it = findAttribute(attName);
	return (it == attributes.end()) ? defaultValue
	                                : StringOp::stringToInt(it->second);
}

bool XMLElement::findAttributeInt(string_ref attName,
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

XMLElement& XMLElement::operator=(const XMLElement& element)
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
		addChild(unique_ptr<XMLElement>(new XMLElement(**it)));
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
		nullptr, reinterpret_cast<const xmlChar*>(str.c_str()));
	string result = reinterpret_cast<const char*>(buffer);
	xmlFree(buffer);
	return result;
}

static unique_ptr<FileContext> lastSerializedFileContext;
unique_ptr<FileContext> XMLElement::getLastSerializedFileContext()
{
	return std::move(lastSerializedFileContext); // this also sets value to nullptr;
}
// version 1: initial version
// version 2: removed 'context' tag
//            also removed 'parent', but that was never serialized
template<typename Archive>
void XMLElement::serialize(Archive& ar, unsigned version)
{
	// note: In the past attributes were stored in a map instead of a
	//       vector. To keep backwards compatible with the serialized
	//       format, we still convert attributes to this format.
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
			addChild(unique_ptr<XMLElement>(*it));
		}
	}
	if (ar.versionBelow(version, 2)) {
		assert(ar.isLoader());
		unique_ptr<FileContext> context;
		ar.serialize("context", context);
		if (context.get()) {
			assert(!lastSerializedFileContext.get());
			lastSerializedFileContext = std::move(context);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(XMLElement);


} // namespace openmsx
