#ifndef XMLELEMENT_HH
#define XMLELEMENT_HH

#include "serialize_meta.hh"
#include <utility>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace openmsx {

class FileContext;

class XMLElement
{
public:
	//
	// Basic functions
	//

	// Construction.
	//  (copy, assign, move, destruct are default)
	XMLElement() = default;
	XMLElement(XMLElement&&) = default;
	XMLElement& operator=(const XMLElement&) = default;
	XMLElement& operator=(XMLElement&&) = default;

	//workaround msvc bug(?)
	//XMLElement(const XMLElement&) = default;
	XMLElement(const XMLElement& e)
		: name(e.name), data(e.data)
		, children(e.children), attributes(e.attributes) {}

	// needed because the template below hides this version
	XMLElement(XMLElement& x) : XMLElement(const_cast<const XMLElement&>(x)) {}

	template<typename String>
	explicit XMLElement(String&& name_)
		: name(std::forward<String>(name_)) {}

	template<typename String1, typename String2>
	XMLElement(String1&& name_, String2&& data_)
		: name(std::forward<String1>(name_)), data(std::forward<String2>(data_)) {}

	// name
	[[nodiscard]] std::string_view getName() const { return name; }
	void clearName() { name.clear(); }

	template<typename String>
	void setName(String&& name_) { name = std::forward<String>(name_); }

	// data
	[[nodiscard]] std::string_view getData() const { return data; }

	template<typename String>
	void setData(String&& data_) {
		assert(children.empty()); // no mixed-content elements
		data = std::forward<String>(data_);
	}

	// attribute
	template<typename String1, typename String2>
	void addAttribute(String1&& attrName, String2&& value) {
		assert(!hasAttribute(attrName));
		attributes.emplace_back(std::forward<String1>(attrName),
		                        std::forward<String2>(value));
	}

	template<typename String1, typename String2>
	void setAttribute(String1&& attrName, String2&& value) {
		auto it = getAttributeIter(attrName);
		if (it != end(attributes)) {
			it->second = std::forward<String2>(value);
		} else {
			attributes.emplace_back(std::forward<String1>(attrName),
			                        std::forward<String2>(value));
		}
	}

	void removeAttribute(std::string_view name);
	[[nodiscard]] bool hasAttribute(std::string_view name) const;
	[[nodiscard]] std::string_view getAttributeValue(std::string_view attrName) const;
	[[nodiscard]] std::string_view getAttributeValue(std::string_view attrName,
	                                                 std::string_view defaultValue) const;
	// Returns ptr to attribute value, or nullptr when not found.
	[[nodiscard]] const std::string* findAttribute(std::string_view attrName) const;

	// child
	using Children = std::vector<XMLElement>;

	//  note: returned XMLElement& is validated on the next addChild() call
	template<typename String>
	XMLElement& addChild(String&& childName) {
		return children.emplace_back(std::forward<String>(childName));
	}

	template<typename String1, typename String2>
	XMLElement& addChild(String1&& childName, String2&& childData) {
		return children.emplace_back(std::forward<String1>(childName),
		                             std::forward<String2>(childData));
	}

	void removeChild(const XMLElement& child);
	[[nodiscard]] const Children& getChildren() const { return children; }
	[[nodiscard]] bool hasChildren() const { return !children.empty(); }

	//
	// Convenience functions
	//

	// attribute
	[[nodiscard]] bool getAttributeValueAsBool(std::string_view attrName,
	                                           bool defaultValue) const;
	[[nodiscard]] int getAttributeValueAsInt(std::string_view attrName,
	                                         int defaultValue) const;
	[[nodiscard]] bool findAttributeInt(std::string_view attrName,
	                                    unsigned& result) const;

	// child
	[[nodiscard]] const XMLElement* findChild(std::string_view childName) const;
	[[nodiscard]] XMLElement* findChild(std::string_view childName);
	[[nodiscard]] const XMLElement& getChild(std::string_view childName) const;
	[[nodiscard]] XMLElement& getChild(std::string_view childName);

	[[nodiscard]] const XMLElement* findChildWithAttribute(
		std::string_view childName, std::string_view attrName,
		std::string_view attValue) const;
	[[nodiscard]] XMLElement* findChildWithAttribute(
		std::string_view childName, std::string_view attrName,
		std::string_view attValue);
	[[nodiscard]] const XMLElement* findNextChild(std::string_view name,
	                                              size_t& fromIndex) const;

	[[nodiscard]] std::vector<const XMLElement*> getChildren(std::string_view childName) const;

	template<typename String>
	XMLElement& getCreateChild(String&& childName) {
		if (auto* result = findChild(childName)) {
			return *result;
		}
		return addChild(std::forward<String>(childName));
	}

	template<typename String1, typename String2>
	XMLElement& getCreateChild(String1&& childName, String2&& defaultValue) {
		if (auto* result = findChild(childName)) {
			return *result;
		}
		return addChild(std::forward<String1>(childName),
		                std::forward<String2>(defaultValue));
	}

	template<typename String1, typename String2, typename String3>
	XMLElement& getCreateChildWithAttribute(
		String1&& childName, String2&& attrName, String3&& attValue)
	{
		if (auto* result = findChildWithAttribute(childName, attrName, attValue)) {
			return *result;
		}
		auto& result = addChild(std::forward<String1>(childName));
		result.addAttribute(std::forward<String2>(attrName),
		                    std::forward<String3>(attValue));
		return result;
	}

	[[nodiscard]] std::string_view getChildData(std::string_view childName) const;
	[[nodiscard]] std::string_view getChildData(std::string_view childName,
	                                            std::string_view defaultValue) const;
	[[nodiscard]] bool getChildDataAsBool(std::string_view childName,
	                                      bool defaultValue = false) const;
	[[nodiscard]] int getChildDataAsInt(std::string_view childName,
	                                    int defaultValue) const;

	template<typename String1, typename String2>
	void setChildData(String1&& childName, String2&& value) {
		if (auto* child = findChild(childName)) {
			child->setData(std::forward<String2>(value));
		} else {
			addChild(std::forward<String1>(childName),
			         std::forward<String2>(value));
		}
	}

	void removeAllChildren();

	// various
	[[nodiscard]] std::string dump() const;
	[[nodiscard]] static std::string XMLEscape(std::string_view str);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// For backwards compatibility with older savestates
	static std::unique_ptr<FileContext> getLastSerializedFileContext();

private:
	using Attribute = std::pair<std::string, std::string>;
	using Attributes = std::vector<Attribute>;
	[[nodiscard]] Attributes::iterator getAttributeIter(std::string_view attrName);
	[[nodiscard]] Attributes::const_iterator getAttributeIter(std::string_view attrName) const;
	void dump(std::string& result, unsigned indentNum) const;

	std::string name;
	std::string data;
	Children children;
	Attributes attributes;
};
SERIALIZE_CLASS_VERSION(XMLElement, 2);

} // namespace openmsx

#endif
