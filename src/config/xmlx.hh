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
	virtual ~XMLElement();

	const string& getName() const { return name; }
	const string& getPcData() const { return pcdata; }

	typedef vector<XMLElement*> Children;
	const Children& getChildren() const { return children; }
	typedef map<string, string> Attributes;
	const Attributes& getAttributes() const { return attributes; }

	const string& getAttribute(const string& attName) const;
	const string& getElementPcdata(const string& childName) const;

protected:
	XMLElement();
	XMLElement(xmlNodePtr node);
	void init(xmlNodePtr node);
	
private:
	string name;
	string pcdata;
	Children children;
	Attributes attributes;
};


class XMLDocument : public XMLElement
{
public:
	XMLDocument(const string& filename) throw(XMLException);
	XMLDocument(const ostringstream& stream) throw(XMLException);

private:
	void handleDoc(xmlDocPtr doc);
};


/**
 * XML escape a string
 * ! changes the string in place!
 * returns const reference to changed self
 * for easy chaining in streams
 * sample:
 * //string stest("hello & world");
 * //cout << XML::Escape(stest) << endl;
 */
const string& XMLEscape(string& str);

} // namespace openmsx

#endif // __XMLX_HH__
