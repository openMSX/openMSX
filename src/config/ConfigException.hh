// $Id$

#ifndef __CONFIGEXCEPTION_HH__
#define __CONFIGEXCEPTION_HH__

#include "MSXException.hh"

namespace openmsx {

class ConfigException : public MSXException
{
public:
	ConfigException(const string& descs)
		: MSXException(descs) {}
};

} // namespace openmsx

#endif
