// $Id$

#ifndef PLUGEXCEPTION_HH
#define PLUGEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

/** Thrown when a plug action fails.
  */
class PlugException : public MSXException
{
public:
	explicit PlugException(const std::string& message)
		: MSXException(message) {}
	explicit PlugException(const char*        message)
		: MSXException(message) {}
};

} // namespace openmsx

#endif
