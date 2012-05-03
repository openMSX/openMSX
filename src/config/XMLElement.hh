#ifndef XMLELEMENT_HH
#define XMLELEMENT_HH

#include "serialize_meta.hh"
#include <utility>
#include <string>
#include <vector>
#include <memory>

namespace StringOp { class Builder; }

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
	XMLElement() {}
	explicit XMLElement(string_ref name);
	XMLElement(string_ref name, string_ref data);

	// name
	const std::string& getName() const { return name; }
	void setName(string_ref name);
	void clearName();

	// data
	const std::string& getData() const { return data; }
	void setData(string_ref data);

	// attribute
	void addAttribute(string_ref name, string_ref value);
	void setAttribute(string_ref name, string_ref value);
	void removeAttribute(string_ref name);

	// child
	using Children = std::vector<XMLElement>;
	//  note: returned XMLElement& is validated on the next addChild() call
	XMLElement& addChild(string_ref name);
	XMLElement& addChild(string_ref name, string_ref data);
	void removeChild(const XMLElement& child);
	const Children& getChildren() const { return children; }
	bool hasChildren() const { return !children.empty(); }

	//
	// Convenience functions
	//

	// attribute
	bool hasAttribute(string_ref name) const;
	const std::string& getAttribute(string_ref attName) const;
	string_ref  getAttribute(string_ref attName,
	                         string_ref defaultValue) const;
	bool getAttributeAsBool(string_ref attName,
	                        bool defaultValue = false) const;
	int getAttributeAsInt(string_ref attName,
	                      int defaultValue = 0) const;
	bool findAttributeInt(string_ref attName,
	                      unsigned& result) const;

	// child
	const XMLElement* findChild(string_ref name) const;
	XMLElement* findChild(string_ref name);
	const XMLElement& getChild(string_ref name) const;
	XMLElement& getChild(string_ref name);

	const XMLElement* findChildWithAttribute(
		string_ref name, string_ref attName,
		string_ref attValue) const;
	XMLElement* findChildWithAttribute(
		string_ref name, string_ref attName,
		string_ref attValue);
	const XMLElement* findNextChild(string_ref name,
	                                size_t& fromIndex) const;

	std::vector<const XMLElement*> getChildren(string_ref name) const;

	XMLElement& getCreateChild(string_ref name,
	                           string_ref defaultValue = {});
	XMLElement& getCreateChildWithAttribute(
		string_ref name, string_ref attName,
		string_ref attValue);

	const std::string& getChildData(string_ref name) const;
	string_ref getChildData(string_ref name,
	                        string_ref defaultValue) const;
	bool getChildDataAsBool(string_ref name,
	                        bool defaultValue = false) const;
	int getChildDataAsInt(string_ref name,
	                      int defaultValue = 0) const;
	void setChildData(string_ref name, string_ref value);

	void removeAllChildren();

	// various
	std::string dump() const;
	static std::string XMLEscape(const std::string& str);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// For backwards compatibility with older savestates
	static std::unique_ptr<FileContext> getLastSerializedFileContext();

private:
	using Attribute = std::pair<std::string, std::string>;
	using Attributes = std::vector<Attribute>;
	Attributes::iterator findAttribute(string_ref name);
	Attributes::const_iterator findAttribute(string_ref name) const;
	void dump(StringOp::Builder& result, unsigned indentNum) const;

	std::string name;
	std::string data;
	Children children;
	Attributes attributes;
};
SERIALIZE_CLASS_VERSION(XMLElement, 2);

} // namespace openmsx

#endif
