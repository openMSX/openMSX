// $Id$

#ifndef CONFIGEXCEPTION_HH
#define CONFIGEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class ConfigException : public MSXException
{
public:
	ConfigException(const std::string& desc)
		: MSXException(desc) {}
};

} // namespace openmsx

#endif
