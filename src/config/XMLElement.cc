#include "XMLElement.hh"
#include "StringOp.hh"
#include "FileContext.hh" // for bwcompat
#include "ConfigException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <cassert>
#include <algorithm>

using std::unique_ptr;
using std::string;

namespace openmsx {

XMLElement::XMLElement(string_ref name_)
	: name(name_.str())
{
}

XMLElement::XMLElement(string_ref name_, string_ref data_)
	: name(name_.str())
	, data(data_.str())
{
}

XMLElement& XMLElement::addChild(string_ref name)
{
	children.emplace_back(name);
	return children.back();
}
XMLElement& XMLElement::addChild(string_ref name, string_ref data)
{
	children.emplace_back(name, data);
	return children.back();
}

void XMLElement::removeChild(const XMLElement& child)
{
	assert(std::count_if(children.begin(), children.end(),
		[&](Children::value_type& v) { return &v == &child; }) == 1);
	auto it = std::find_if(children.begin(), children.end(),
		[&](Children::value_type& v) { return &v == &child; });
	assert(it != children.end());
	children.erase(it);
}

XMLElement::Attributes::iterator XMLElement::findAttribute(string_ref name)
{
	return find_if(attributes.begin(), attributes.end(),
	               [&](Attribute& a) { return a.first == name; });
}
XMLElement::Attributes::const_iterator XMLElement::findAttribute(string_ref name) const
{
	return find_if(attributes.begin(), attributes.end(),
	               [&](const Attribute& a) { return a.first == name; });
}

void XMLElement::addAttribute(string_ref name, string_ref value)
{
	assert(findAttribute(name) == attributes.end());
	attributes.emplace_back(name.str(), value.str());
}

void XMLElement::setAttribute(string_ref name, string_ref value)
{
	auto it = findAttribute(name);
	if (it != attributes.end()) {
		it->second = value.str();
	} else {
		attributes.emplace_back(name.str(), value.str());
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
	name = name_.str();
}

void XMLElement::clearName()
{
	name.clear();
}

void XMLElement::setData(string_ref data_)
{
	assert(children.empty()); // no mixed-content elements
	data = data_.str();
}

std::vector<const XMLElement*> XMLElement::getChildren(string_ref name) const
{
	std::vector<const XMLElement*> result;
	for (auto& c : children) {
		if (c.getName() == name) {
			result.push_back(&c);
		}
	}
	return result;
}

XMLElement* XMLElement::findChild(string_ref name)
{
	for (auto& c : children) {
		if (c.getName() == name) {
			return &c;
		}
	}
	return nullptr;
}
const XMLElement* XMLElement::findChild(string_ref name) const
{
	return const_cast<XMLElement*>(this)->findChild(name);
}

const XMLElement* XMLElement::findNextChild(string_ref name,
	                                    size_t& fromIndex) const
{
	for (auto i : xrange(fromIndex, children.size())) {
		if (children[i].getName() == name) {
			fromIndex = i + 1;
			return &children[i];
		}
	}
	for (auto i : xrange(fromIndex)) {
		if (children[i].getName() == name) {
			fromIndex = i + 1;
			return &children[i];
		}
	}
	return nullptr;
}

XMLElement* XMLElement::findChildWithAttribute(string_ref name,
	string_ref attName, string_ref attValue)
{
	for (auto& c : children) {
		if ((c.getName() == name) &&
		    (c.getAttribute(attName) == attValue)) {
			return &c;
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
	if (auto* elem = findChild(name)) {
		return *elem;
	}
	throw ConfigException(StringOp::Builder() <<
		"Missing tag \"" << name << "\".");
}
const XMLElement& XMLElement::getChild(string_ref name) const
{
	return const_cast<XMLElement*>(this)->getChild(name);
}

XMLElement& XMLElement::getCreateChild(string_ref name,
                                       string_ref defaultValue)
{
	if (auto* result = findChild(name)) {
		return *result;
	}
	return addChild(name, defaultValue);
}

XMLElement& XMLElement::getCreateChildWithAttribute(
	string_ref name, string_ref attName,
	string_ref attValue)
{
	if (auto* result = findChildWithAttribute(name, attName, attValue)) {
		return *result;
	}
	auto& result = addChild(name);
	result.addAttribute(attName, attValue);
	return result;
}

const string& XMLElement::getChildData(string_ref name) const
{
	return getChild(name).getData();
}

string_ref XMLElement::getChildData(string_ref name,
                                    string_ref defaultValue) const
{
	auto* child = findChild(name);
	return child ? child->getData() : defaultValue;
}

bool XMLElement::getChildDataAsBool(string_ref name, bool defaultValue) const
{
	auto* child = findChild(name);
	return child ? StringOp::stringToBool(child->getData()) : defaultValue;
}

int XMLElement::getChildDataAsInt(string_ref name, int defaultValue) const
{
	auto* child = findChild(name);
	return child ? StringOp::stringToInt(child->getData()) : defaultValue;
}

void XMLElement::setChildData(string_ref name, string_ref value)
{
	if (auto* child = findChild(name)) {
		child->setData(value);
	} else {
		addChild(name, value);
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
			c.dump(result, indentNum + 2);
		}
		result << indent << "</" << getName() << ">\n";
	}
}

// This routine does the following substitutions:
//  & -> &amp;   must always be done
//  < -> &lt;    must always be done
//  > -> &gt;    always allowed, but must be done when it appears as ]]>
//  ' -> &apos;  always allowed, but must be done inside quoted attribute
//  " -> &quot;  always allowed, but must be done inside quoted attribute
// So to simplify things we always do these 5 substitutions.
string XMLElement::XMLEscape(const string& s)
{
	static const char* const CHARS = "<>&\"'";
	size_t i = s.find_first_of(CHARS);
	if (i == string::npos) return s; // common case, no substitutions

	string result;
	result.reserve(s.size() + 10); // extra space for at least 2 substitutions
	size_t pos = 0;
	while (true) {
		result += s.substr(pos, i - pos);
		switch (s[i]) {
		case '<' : result += "&lt;";   break;
		case '>' : result += "&gt;";   break;
		case '&' : result += "&amp;";  break;
		case '"' : result += "&quot;"; break;
		case '\'': result += "&apos;"; break;
		default: UNREACHABLE;
		}
		pos = i + 1;
		i = s.find_first_of(CHARS, pos);
		if (i == string::npos) {
			result += s.substr(pos);
			return result;
		}
	}
}

static unique_ptr<FileContext> lastSerializedFileContext;
unique_ptr<FileContext> XMLElement::getLastSerializedFileContext()
{
	return std::move(lastSerializedFileContext); // this also sets value to nullptr;
}
// version 1: initial version
// version 2: removed 'context' tag
//            also removed 'parent', but that was never serialized
//        2b: (no need to increase version) name and data members are
//            serialized as normal members instead of constructor parameters
//        2c: (no need to increase version) attributes were initially stored as
//            map<string, string>, later this was changed to
//            vector<pair<string, string>>. To keep bw-compat the serialize()
//            method converted between these two formats. Though (by luck) in
//            the XML output both datastructures are serialized to the same
//            format, so we can drop this conversion step without breaking
//            bw-compat.
template<typename Archive>
void XMLElement::serialize(Archive& ar, unsigned version)
{
	ar.serialize("name", name);
	ar.serialize("data", data);
	ar.serialize("attributes", attributes);
	ar.serialize("children", children);

	if (ar.versionBelow(version, 2)) {
		assert(ar.isLoader());
		unique_ptr<FileContext> context;
		ar.serialize("context", context);
		if (context) {
			assert(!lastSerializedFileContext);
			lastSerializedFileContext = std::move(context);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(XMLElement);

} // namespace openmsx
