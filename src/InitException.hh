#ifndef INITEXCEPTION_HH
#define INITEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

/** Thrown when a subsystem initialisation fails.
  * For example: opening video surface, opening audio output etc.
  */
class InitException final : public MSXException
{
public:
	explicit InitException(string_ref message_)
		: MSXException(message_) {}
};

} // namespace openmsx

#endif
