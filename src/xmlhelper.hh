// $Id$

#ifndef __XMLHELPER_HH__
#define __XMLHELPER_HH__

#include <string>

// forward declarations
class XMLNode;

template <class T> class XMLHelper
{
public:
	XMLHelper(XMLNode *anode);
	void checkName(const std::string &name);
	void checkProperty(const std::string &name);
	const string &getProperty(const std::string &name);
	const string &getProperty(const std::string &name, const std::string &defaultValue);

private:
	XMLHelper(const XMLHelper &) {} // block usage
	XMLHelper &operator=(const XMLHelper &) {} // block usage

	XMLNode *node;
	const string emptyString;
};

#endif
