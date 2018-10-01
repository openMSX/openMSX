#ifndef PLUGEXCEPTION_HH
#define PLUGEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

/** Thrown when a plug action fails.
  */
class PlugException final : public MSXException
{
public:
	using MSXException::MSXException;
};

} // namespace openmsx

#endif
