// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include "xmlx.hh"

namespace openmsx {

class FileContext;

class MSXConfig : public XMLElement
{
public:
	static void loadConfig(XMLElement& root, const FileContext& context,
	                       auto_ptr<XMLElement> elem);

protected:
	MSXConfig(const string& name);
	static void handleDoc(XMLElement& root, const XMLDocument& doc,
	                      FileContext& context);
};

} // namespace openmsx

#endif
