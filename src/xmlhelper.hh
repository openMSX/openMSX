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

private:
	XMLHelper(const XMLHelper &) {} // block usage
	XMLHelper &operator=(const XMLHelper &) {} // block usage

	XMLNode *node;
};

#endif
