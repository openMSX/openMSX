// $Id$

#ifndef __MSXCONFIG_HH__
#define __MSXCONFIG_HH__

#include <string>
#include <vector>
#include "xmlx.hh"

using std::string;
using std::vector;

namespace openmsx {

class FileContext;

class MSXConfig : public XMLElement
{
public:
	void loadConfig(const FileContext& context, auto_ptr<XMLElement> elem);
	const XMLElement* findConfigById(const string& id);

protected:
	void handleDoc(const XMLDocument& doc, FileContext& context);
};

} // namespace openmsx

#endif
