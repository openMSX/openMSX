// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include "xmlx.hh"

namespace openmsx {

class FileContext;

class MSXConfig : public XMLElement
{
public:
	void loadConfig(const FileContext& context, auto_ptr<XMLElement> elem);

protected:
	void handleDoc(const XMLDocument& doc, FileContext& context);
};

} // namespace openmsx

#endif
