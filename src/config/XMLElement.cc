#include "XMLElement.hh"
#include "StringOp.hh"
#include "FileContext.hh" // for bwcompat
#include "ConfigException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <cassert>
#include <algorithm>

using std::unique_ptr;
using std::string;

namespace openmsx {

XMLElement::XMLElement(string_view name_)
	: name(name_.str())
{
}

XMLElement::XMLElement(string_view name_, string_view data_)
	: name(name_.str())
	, data(data_.str())
{
}

XMLElement& XMLElement::addChild(string_view childName)
{
	children.emplace_back(childName);
	return children.back();
}
XMLElement& XMLElement::addChild(string_view childName, string_view childData)
{
	children.emplace_back(childName, childData);
	return children.back();
}

void XMLElement::removeChild(const XMLElement& child)
{
	children.erase(rfind_if_unguarded(children,
		[&](Children::value_type& v) { return &v == &child; }));
}

XMLElement::Attributes::iterator XMLElement::findAttribute(string_view attrName)
{
	return find_if(begin(attributes), end(attributes),
	               [&](Attribute& a) { return a.first == attrName; });
}
XMLElement::Attributes::const_iterator XMLElement::findAttribute(string_view attrName) const
{
	return find_if(begin(attributes), end(attributes),
	               [&](const Attribute& a) { return a.first == attrName; });
}

void XMLElement::addAttribute(string_view attrName, string_view value)
{
	assert(findAttribute(attrName) == end(attributes));
	attributes.emplace_back(attrName.str(), value.str());
}

void XMLElement::setAttribute(string_view attrName, string_view value)
{
	auto it = findAttribute(attrName);
	if (it != end(attributes)) {
		it->second = value.str();
	} else {
		attributes.emplace_back(attrName.str(), value.str());
	}
}

void XMLElement::removeAttribute(string_view attrName)
{
	auto it = findAttribute(attrName);
	if (it != end(attributes)) {
		attributes.erase(it);
	}
}

void XMLElement::setName(string_view newName)
{
	name = newName.str();
}

void XMLElement::clearName()
{
	name.clear();
}

void XMLElement::setData(string_view newData)
{
	assert(children.empty()); // no mixed-content elements
	data = newData.str();
}

std::vector<const XMLElement*> XMLElement::getChildren(string_view childName) const
{
	std::vector<const XMLElement*> result;
	for (auto& c : children) {
		if (c.getName() == childName) {
			result.push_back(&c);
		}
	}
	return result;
}

XMLElement* XMLElement::findChild(string_view childName)
{
	for (auto& c : children) {
		if (c.getName() == childName) {
			return &c;
		}
	}
	return nullptr;
}
const XMLElement* XMLElement::findChild(string_view childName) const
{
	return const_cast<XMLElement*>(this)->findChild(childName);
}

const XMLElement* XMLElement::findNextChild(string_view childName,
	                                    size_t& fromIndex) const
{
	for (auto i : xrange(fromIndex, children.size())) {
		if (children[i].getName() == childName) {
			fromIndex = i + 1;
			return &children[i];
		}
	}
	for (auto i : xrange(fromIndex)) {
		if (children[i].getName() == childName) {
			fromIndex = i + 1;
			return &children[i];
		}
	}
	return nullptr;
}

XMLElement* XMLElement::findChildWithAttribute(string_view childName,
	string_view attName, string_view attValue)
{
	for (auto& c : children) {
		if ((c.getName() == childName) &&
		    (c.getAttribute(attName) == attValue)) {
			return &c;
		}
	}
	return nullptr;
}

const XMLElement* XMLElement::findChildWithAttribute(string_view childName,
	string_view attName, string_view attValue) const
{
	return const_cast<XMLElement*>(this)->findChildWithAttribute(
		childName, attName, attValue);
}

XMLElement& XMLElement::getChild(string_view childName)
{
	if (auto* elem = findChild(childName)) {
		return *elem;
	}
	throw ConfigException("Missing tag \"", childName, "\".");
}
const XMLElement& XMLElement::getChild(string_view childName) const
{
	return const_cast<XMLElement*>(this)->getChild(childName);
}

XMLElement& XMLElement::getCreateChild(string_view childName,
                                       string_view defaultValue)
{
	if (auto* result = findChild(childName)) {
		return *result;
	}
	return addChild(childName, defaultValue);
}

XMLElement& XMLElement::getCreateChildWithAttribute(
	string_view childName, string_view attName,
	string_view attValue)
{
	if (auto* result = findChildWithAttribute(childName, attName, attValue)) {
		return *result;
	}
	auto& result = addChild(childName);
	result.addAttribute(attName, attValue);
	return result;
}

const string& XMLElement::getChildData(string_view childName) const
{
	return getChild(childName).getData();
}

string_view XMLElement::getChildData(string_view childName,
                                    string_view defaultValue) const
{
	auto* child = findChild(childName);
	return child ? child->getData() : defaultValue;
}

bool XMLElement::getChildDataAsBool(string_view childName, bool defaultValue) const
{
	auto* child = findChild(childName);
	return child ? StringOp::stringToBool(child->getData()) : defaultValue;
}

int XMLElement::getChildDataAsInt(string_view childName, int defaultValue) const
{
	auto* child = findChild(childName);
	return child ? StringOp::stringToInt(child->getData()) : defaultValue;
}

void XMLElement::setChildData(string_view childName, string_view value)
{
	if (auto* child = findChild(childName)) {
		child->setData(value);
	} else {
		addChild(childName, value);
	}
}

void XMLElement::removeAllChildren()
{
	children.clear();
}

bool XMLElement::hasAttribute(string_view attrName) const
{
	return findAttribute(attrName) != end(attributes);
}

const string& XMLElement::getAttribute(string_view attName) const
{
	auto it = findAttribute(attName);
	if (it == end(attributes)) {
		throw ConfigException("Missing attribute \"", attName, "\".");
	}
	return it->second;
}

string_view XMLElement::getAttribute(string_view attName,
	                            string_view defaultValue) const
{
	auto it = findAttribute(attName);
	return (it == end(attributes)) ? defaultValue : it->second;
}

bool XMLElement::getAttributeAsBool(string_view attName,
                                    bool defaultValue) const
{
	auto it = findAttribute(attName);
	return (it == end(attributes)) ? defaultValue
	                                : StringOp::stringToBool(it->second);
}

int XMLElement::getAttributeAsInt(string_view attName,
                                  int defaultValue) const
{
	auto it = findAttribute(attName);
	return (it == end(attributes)) ? defaultValue
	                                : StringOp::stringToInt(it->second);
}

bool XMLElement::findAttributeInt(string_view attName,
                                  unsigned& result) const
{
	auto it = findAttribute(attName);
	if (it != end(attributes)) {
		result = StringOp::stringToInt(it->second);
		return true;
	} else {
		return false;
	}
}

string XMLElement::dump() const
{
	string result;
	dump(result, 0);
	return result;
}

void XMLElement::dump(string& result, unsigned indentNum) const
{
	strAppend(result, spaces(indentNum), '<', getName());
	for (auto& p : attributes) {
		strAppend(result, ' ', p.first,
		          "=\"", XMLEscape(p.second), '"');
	}
	if (children.empty()) {
		if (data.empty()) {
			strAppend(result, "/>\n");
		} else {
			strAppend(result, '>', XMLEscape(data), "</",
			          getName(), ">\n");
		}
	} else {
		strAppend(result, ">\n");
		for (auto& c : children) {
			c.dump(result, indentNum + 2);
		}
		strAppend(result, spaces(indentNum), "</", getName(), ">\n");
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
	do {
		strAppend(result, string_view(s).substr(pos, i - pos));
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
	} while (i != string::npos);
	strAppend(result, string_view(s).substr(pos));
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
