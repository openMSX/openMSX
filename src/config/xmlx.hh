// $Id$

#ifndef __XMLX_HH__
#define __XMLX_HH__

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <libxml/parser.h>
#include "MSXException.hh"

namespace openmsx {

class FileContext;

class XMLException: public MSXException
{
public:
	XMLException(const std::string& msg);
};


class XMLElement
{
public:
	XMLElement(const std::string& name, const std::string& data = "");
	XMLElement(const XMLElement& element);
	~XMLElement();
	
	const XMLElement& operator=(const XMLElement& element);

	XMLElement* getParent();
	const XMLElement* getParent() const;
	
	void addChild(std::auto_ptr<XMLElement> child);
	void deleteChild(const XMLElement& child);
	void addAttribute(const std::string& name, const std::string& value);
	
	const std::string& getName() const { return name; }
	const std::string& getData() const { return data; }
	bool getDataAsBool() const;
	int getDataAsInt() const;
	double getDataAsDouble() const;
	void setData(const std::string& data_);

	typedef std::vector<XMLElement*> Children;
	const Children& getChildren() const { return children; }
	void getChildren(const std::string& name, Children& result) const;
	const XMLElement* findChild(const std::string& name) const;
	XMLElement* findChild(const std::string& name);
	const XMLElement& getChild(const std::string& name) const;
	XMLElement& getChild(const std::string& name);

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
	
	typedef std::map<std::string, std::string> Attributes;
	bool hasAttribute(const std::string& name) const;
	const Attributes& getAttributes() const;
	const std::string& getAttribute(const std::string& attName) const;
	const std::string getAttribute(const std::string& attName,
	                          const std::string defaultValue) const;
	bool getAttributeAsBool(const std::string& attName,
	                        bool defaultValue = false) const;
	int getAttributeAsInt(const std::string& attName,
	                      int defaultValue = 0) const;

	const std::string& getId() const;
	
	void setFileContext(std::auto_ptr<FileContext> context);
	FileContext& getFileContext() const;

	std::string dump() const;
	
	static std::string makeUnique(const std::string& str);
	static std::string XMLEscape(const std::string& str);

protected:
	XMLElement();
	XMLElement(xmlNodePtr node);
	void init(xmlNodePtr node);
	
private:
	void dump(std::string& result, unsigned indentNum) const;
	
	std::string name;
	std::string data;
	Children children;
	Attributes attributes;
	XMLElement* parent;
	std::auto_ptr<FileContext> context;

	static std::map<std::string, unsigned> idMap;
};


class XMLDocument : public XMLElement
{
public:
	XMLDocument(const std::string& filename, const std::string& systemID);
};


} // namespace openmsx

#endif // __XMLX_HH__
