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

using std::map;
using std::vector;
using std::string;
using std::auto_ptr;
using std::ostringstream;

namespace openmsx
{

class FileContext;

class XMLException: public MSXException
{
public:
	XMLException(const string& msg);
};


class XMLElement
{
public:
	XMLElement(const string& name, const string& data = "");
	XMLElement(const XMLElement& element);
	~XMLElement();

	XMLElement* getParent();
	const XMLElement* getParent() const;
	
	void addChild(auto_ptr<XMLElement> child);
	void addAttribute(const string& name, const string& value);
	
	const string& getName() const { return name; }
	const string& getData() const { return data; }

	typedef vector<XMLElement*> Children;
	const Children& getChildren() const { return children; }
	void getChildren(const string& name, Children& result) const;
	const XMLElement* findChild(const string& name) const;
	const XMLElement& getChild(const string& name) const;
	
	const string& getChildData(const string& name) const;
	string getChildData(const string& name,
	                    const string& defaultValue) const;
	bool getChildDataAsBool(const string& name,
	                        bool defaultValue = false) const;
	int getChildDataAsInt(const string& name,
	                      int defaultValue = 0) const;
	
	typedef map<string, string> Attributes;
	const Attributes& getAttributes() const { return attributes; }
	const string& getAttribute(const string& attName) const;
	const string getAttribute(const string& attName,
	                          const string defaultValue) const;

	const string& getId() const;
	
	void setFileContext(auto_ptr<FileContext> context);
	FileContext& getFileContext() const;
	
	const XMLElement& operator=(const XMLElement& element);

protected:
	XMLElement();
	XMLElement(xmlNodePtr node);
	void init(xmlNodePtr node);
	
private:
	string name;
	string data;
	Children children;
	Attributes attributes;
	XMLElement* parent;
	auto_ptr<FileContext> context;
};


class XMLDocument : public XMLElement
{
public:
	XMLDocument(const string& filename);
	XMLDocument(const ostringstream& stream);

private:
	void handleDoc(xmlDocPtr doc);
};


/**
 * XML escape a string
 */
string XMLEscape(const string& str);

} // namespace openmsx

#endif // __XMLX_HH__
