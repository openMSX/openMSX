// $Id$

#ifndef XMLELEMENT_HH
#define XMLELEMENT_HH

#include "serialize_constr.hh"
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

	// construction, destruction, copy, assign
	explicit XMLElement(const std::string& name);
	explicit XMLElement(const char* name);
	XMLElement(const std::string& name, const std::string& data);
	XMLElement(const char* name, const char* data);
	XMLElement(const XMLElement& element);
	const XMLElement& operator=(const XMLElement& element);
	~XMLElement();

	// name
	const std::string& getName() const { return name; }
	void setName(const std::string& name);
	void clearName();

	// data
	const std::string& getData() const { return data; }
	void setData(const std::string& data);

	// attribute
	void addAttribute(const char* name, const std::string& value);
	void setAttribute(const char* name, const std::string& value);
	void removeAttribute(const char* name);

	// parent
	XMLElement* getParent();
	const XMLElement* getParent() const;

	// child
	typedef std::vector<XMLElement*> Children;
	void addChild(std::auto_ptr<XMLElement> child);
	std::auto_ptr<XMLElement> removeChild(const XMLElement& child);
	const Children& getChildren() const { return children; }
	bool hasChildren() const { return !children.empty(); }

	// filecontext
	void setFileContext(std::auto_ptr<FileContext> context);
	FileContext& getFileContext() const;

	//
	// Convenience functions
	//

	// data
	bool getDataAsBool() const;
	int getDataAsInt() const;
	double getDataAsDouble() const;

	// attribute
	bool hasAttribute(const char* name) const;
	const std::string& getAttribute(const char* attName) const;
	const std::string  getAttribute(const char* attName,
	                                const char* defaultValue) const;
	bool getAttributeAsBool(const char* attName,
	                        bool defaultValue = false) const;
	int getAttributeAsInt(const char* attName,
	                      int defaultValue = 0) const;
	bool findAttributeInt(const char* attName,
	                      unsigned& result) const;
	const std::string& getId() const;

	// child
	const XMLElement* findChild(const char* name) const;
	XMLElement* findChild(const char* name);
	const XMLElement& getChild(const char* name) const;
	XMLElement& getChild(const char* name);

	const XMLElement* findChildWithAttribute(
		const char* name, const char* attName,
		const std::string& attValue) const;
	XMLElement* findChildWithAttribute(
		const char* name, const char* attName,
		const std::string& attValue);
	const XMLElement* findNextChild(const char* name,
	                                unsigned& fromIndex) const;

	void getChildren(const std::string& name, Children& result) const;
	void getChildren(const char* name, Children& result) const;

	XMLElement& getCreateChild(const char* name,
	                           const std::string& defaultValue = "");
	XMLElement& getCreateChildWithAttribute(
		const char* name, const char* attName,
		const std::string& attValue);

	const std::string& getChildData(const char* name) const;
	std::string getChildData(const char* name,
	                         const char* defaultValue) const;
	bool getChildDataAsBool(const char* name,
	                        bool defaultValue = false) const;
	int getChildDataAsInt(const char* name,
	                      int defaultValue = 0) const;
	void setChildData(const char* name, const std::string& value);

	void removeAllChildren();

	// various
	std::string dump() const;
	static std::string XMLEscape(const std::string& str);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	typedef std::pair<std::string, std::string> Attribute;
	typedef std::vector<Attribute> Attributes;
	Attributes::iterator findAttribute(const char* name);
	Attributes::const_iterator findAttribute(const char* name) const;
	void dump(StringOp::Builder& result, unsigned indentNum) const;

	std::string name;
	std::string data;
	Children children;
	Attributes attributes;
	XMLElement* parent;
	std::auto_ptr<FileContext> context;
};

template<> struct SerializeConstructorArgs<XMLElement>
{
	typedef Tuple<std::string, std::string> type;
	template<typename Archive> void save(Archive& ar, const XMLElement& xml)
	{
		ar.serialize("name", xml.getName());
		ar.serialize("data", xml.getData());
	}
	template<typename Archive> type load(Archive& ar, unsigned /*version*/)
	{
		std::string name, data;
		ar.serialize("name", name);
		ar.serialize("data", data);
		return make_tuple(name, data);
	}
};

} // namespace openmsx

#endif
