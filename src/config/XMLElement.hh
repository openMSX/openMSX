// $Id$

#ifndef XMLELEMENT_HH
#define XMLELEMENT_HH

#include <map>
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class FileContext;
class XMLElementListener;

class XMLElement
{
public:
	//
	// Basic functions
	//

	// construction, destruction, copy, assign
	XMLElement(const std::string& name, const std::string& data = "");
	XMLElement(const XMLElement& element);
	const XMLElement& operator=(const XMLElement& element);
	virtual ~XMLElement();

	// name
	const std::string& getName() const { return name; }
	void setName(const std::string& name);

	// data
	const std::string& getData() const { return data; }
	void setData(const std::string& data);

	// attribute
	typedef std::map<std::string, std::string> Attributes;
	void addAttribute(const std::string& name, const std::string& value);
	const Attributes& getAttributes() const;

	// parent
	XMLElement* getParent();
	const XMLElement* getParent() const;

	// child
	typedef std::vector<XMLElement*> Children;
	void addChild(std::auto_ptr<XMLElement> child);
	std::auto_ptr<XMLElement> removeChild(const XMLElement& child);
	const Children& getChildren() const { return children; }

	// filecontext
	void setFileContext(std::auto_ptr<FileContext> context);
	FileContext& getFileContext() const;

	// listener
	void addListener(XMLElementListener& listener);
	void removeListener(XMLElementListener& listener);

	//
	// Convenience functions
	//

	// data
	bool getDataAsBool() const;
	int getDataAsInt() const;
	double getDataAsDouble() const;

	// attribute
	bool hasAttribute(const std::string& name) const;
	const std::string& getAttribute(const std::string& attName) const;
	const std::string getAttribute(const std::string& attName,
	                          const std::string defaultValue) const;
	bool getAttributeAsBool(const std::string& attName,
	                        bool defaultValue = false) const;
	int getAttributeAsInt(const std::string& attName,
	                      int defaultValue = 0) const;
	const std::string& getId() const;

	// child
	const XMLElement* findChild(const std::string& name) const;
	XMLElement* findChild(const std::string& name);
	const XMLElement& getChild(const std::string& name) const;
	XMLElement* findChildWithAttribute(
		const std::string& name, const std::string& attName,
		const std::string& attValue);

	XMLElement& getChild(const std::string& name);
	void getChildren(const std::string& name, Children& result) const;

	XMLElement& getCreateChild(const std::string& name,
	                           const std::string& defaultValue = "");
	XMLElement& getCreateChildWithAttribute(
		const std::string& name, const std::string& attName,
		const std::string& attValue, const std::string& defaultValue = "");

	const std::string& getChildData(const std::string& name) const;
	std::string getChildData(const std::string& name,
	                    const std::string& defaultValue) const;
	bool getChildDataAsBool(const std::string& name,
	                        bool defaultValue = false) const;
	int getChildDataAsInt(const std::string& name,
	                      int defaultValue = 0) const;

	// various
	std::string dump() const;
	void merge(const XMLElement& source);
	bool isShallowEqual(const XMLElement& other) const;

	static std::string XMLEscape(const std::string& str);

private:
	void dump(std::string& result, unsigned indentNum) const;

	std::string name;
	std::string data;
	Children children;
	Attributes attributes;
	XMLElement* parent;
	std::auto_ptr<FileContext> context;
	typedef std::vector<XMLElementListener*> Listeners;
	Listeners listeners;
	int notifyInProgress;
};

} // namespace openmsx

#endif
