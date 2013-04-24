#ifndef CONFIGEXCEPTION_HH
#define CONFIGEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class ConfigException : public MSXException
{
public:
	explicit ConfigException(string_ref message)
		: MSXException(message) {}
};

} // namespace openmsx

#endif
