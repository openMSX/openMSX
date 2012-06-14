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
	explicit PlugException(string_ref message)
		: MSXException(message) {}
};

} // namespace openmsx

#endif
