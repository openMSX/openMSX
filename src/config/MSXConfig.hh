// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include "xmlx.hh"

namespace openmsx {

class FileContext;

class MSXConfig : public XMLElement
{
protected:
	MSXConfig(const string& name);
	static void handleDoc(XMLElement& root, const XMLDocument& doc,
	                      FileContext& context);
};

} // namespace openmsx

#endif
