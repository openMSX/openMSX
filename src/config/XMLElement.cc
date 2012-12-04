// $Id$

#include "XMLElement.hh"
#include "StringOp.hh"
#include "FileContext.hh" // for bwcompat
#include "ConfigException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "memory.hh"
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
	children.push_back(std::move(child));
}

unique_ptr<XMLElement> XMLElement::removeChild(const XMLElement& child)
{
	assert(std::count_if(children.begin(), children.end(),
		[&](Children::value_type& v) { return v.get() == &child; }) == 1);
	auto it = std::find_if(children.begin(), children.end(),
		[&](Children::value_type& v) { return v.get() == &child; });
	assert(it != children.end());
	auto result = std::move(*it);
	children.erase(it);
	return result;
}

XMLElement::Attributes::iterator XMLElement::findAttribute(string_ref name)
{
	for (auto it = attributes.begin(); it != attributes.end(); ++it) {
		if (it->first == name) {
			return it;
		}
	}
	return attributes.end();
}
XMLElement::Attributes::const_iterator XMLElement::findAttribute(string_ref name) const
{
	for (auto it = attributes.begin(); it != attributes.end(); ++it) {
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
	auto it = findAttribute(name);
	if (it != attributes.end()) {
		it->second.assign(value.data(), value.size());
	} else {
		attributes.push_back(make_pair(name.str(), value.str()));
	}
}

void XMLElement::removeAttribute(string_ref name)
{
	auto it = findAttribute(name);
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

std::vector<XMLElement*> XMLElement::getChildren(string_ref name) const
{
	std::vector<XMLElement*> result;
	for (auto& c : children) {
		if (c->getName() == name) {
			result.push_back(c.get());
		}
	}
	return result;
}

XMLElement* XMLElement::findChild(string_ref name)
{
	for (auto& c : children) {
		if (c->getName() == name) {
			return c.get();
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
			return children[i].get();
		}
	}
	for (unsigned i = 0; i < fromIndex; ++i) {
		if (children[i]->getName() == name) {
			fromIndex = i + 1;
			return children[i].get();
		}
	}
	return nullptr;
}

XMLElement* XMLElement::findChildWithAttribute(string_ref name,
	string_ref attName, string_ref attValue)
{
	for (auto& c : children) {
		if ((c->getName() == name) &&
		    (c->getAttribute(attName) == attValue)) {
			return c.get();
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
		auto p = make_unique<XMLElement>(name, defaultValue);
		result = p.get();
		addChild(std::move(p));
	}
	return *result;
}

XMLElement& XMLElement::getCreateChildWithAttribute(
	string_ref name, string_ref attName,
	string_ref attValue)
{
	XMLElement* result = findChildWithAttribute(name, attName, attValue);
	if (!result) {
		auto p = make_unique<XMLElement>(name);
		result = p.get();
		p->addAttribute(attName, attValue);
		addChild(std::move(p));
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
		addChild(make_unique<XMLElement>(name, value));
	}
}

void XMLElement::removeAllChildren()
{
	children.clear();
}

bool XMLElement::hasAttribute(string_ref name) const
{
	return findAttribute(name) != attributes.end();
}

const string& XMLElement::getAttribute(string_ref attName) const
{
	auto it = findAttribute(attName);
	if (it == attributes.end()) {
		throw ConfigException("Missing attribute \"" +
		                      attName + "\".");
	}
	return it->second;
}

string_ref XMLElement::getAttribute(string_ref attName,
	                            string_ref defaultValue) const
{
	auto it = findAttribute(attName);
	return (it == attributes.end()) ? defaultValue : it->second;
}

bool XMLElement::getAttributeAsBool(string_ref attName,
                                    bool defaultValue) const
{
	auto it = findAttribute(attName);
	return (it == attributes.end()) ? defaultValue
	                                : StringOp::stringToBool(it->second);
}

int XMLElement::getAttributeAsInt(string_ref attName,
                                  int defaultValue) const
{
	auto it = findAttribute(attName);
	return (it == attributes.end()) ? defaultValue
	                                : StringOp::stringToInt(it->second);
}

bool XMLElement::findAttributeInt(string_ref attName,
                                  unsigned& result) const
{
	auto it = findAttribute(attName);
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
	for (auto& c : element.children) {
		addChild(make_unique<XMLElement>(*c));
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
	for (auto& p : attributes) {
		result << ' ' << p.first
		       << "=\"" << XMLEscape(p.second) << '"';
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
		for (auto& c : children) {
			c->dump(result, indentNum + 2);
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
	} else {
		AttributesMap tmpAtt;
		ar.serialize("attributes", tmpAtt);
		for (auto& p : tmpAtt) {
			// TODO "string -> char* -> string" conversion can
			//       be optimized
			addAttribute(p.first.c_str(), p.second);
		}
	}

	ar.serialize("children", children);
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
