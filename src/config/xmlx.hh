// $Id$

#ifndef __XMLX_HH__
#define __XMLX_HH__

#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <libxml/parser.h>
#include "MSXException.hh"

using std::map;
using std::vector;
using std::string;
using std::ostringstream;

namespace openmsx
{

class XMLException: public MSXException
{
public:
	XMLException(const string& msg);
};


class XMLElement
{
public:
	XMLElement(const string& name, const string& data = "");
	~XMLElement();

	void addChild(XMLElement* child);
	void addAttribute(const string& name, const string& value);
	
	const string& getName() const { return name; }
	const string& getData() const { return data; }

	typedef vector<XMLElement*> Children;
	const Children& getChildren() const { return children; }
	void getChildren(const string& name, Children& result) const;
	const XMLElement* getChild(const string& name) const;
	const string& getChildData(const string& name) const;
	
	typedef map<string, string> Attributes;
	const Attributes& getAttributes() const { return attributes; }
	const string& getAttribute(const string& attName) const;
	
	XMLElement(const XMLElement& element);
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
};


class XMLDocument : public XMLElement
{
public:
	XMLDocument(const string& filename);
	XMLDocument(const ostringstream& stream);

	XMLDocument(const XMLDocument& document);
	const XMLDocument& operator=(const XMLDocument& document);

private:
	void handleDoc(xmlDocPtr doc);
};


/**
 * XML escape a string
 */
string XMLEscape(const string& str);

} // namespace openmsx

#endif // __XMLX_HH__
