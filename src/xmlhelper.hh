// $Id$

#ifndef __XMLHELPER_HH__
#define __XMLHELPER_HH__

#include <string>
#include <list>

// forward declarations
class XMLNode;

template <class T> class XMLHelper
{
public:
	// (re)init
	XMLHelper(XMLNode *anode);
	void setNode(XMLNode *anode);

	// name
	void checkName(const std::string &name);
	bool justCheckName(const std::string &name);
	void checkName(const std::list<std::string> &nameList);

	// properties
	void checkProperty(const std::string &name);
	const std::string &getProperty(const std::string &name);
	const std::string &getProperty(const std::string &name, const std::string &defaultValue);

	// children
	void checkChildrenAtLeast(unsigned int num);
	unsigned int childrenSize();

	// content nodes
	void checkContentNode();
	const std::string &getContent();

	// debugging
	void dump(int depth=0);

private:
	XMLHelper(const XMLHelper &) {} // block usage
	XMLHelper &operator=(const XMLHelper &) {} // block usage

	XMLNode *node;
	const std::string emptyString;
};

#endif
