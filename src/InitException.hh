// $Id$

#ifndef __INITEXCEPTION_HH__
#define __INITEXCEPTION_HH__

#include "MSXException.hh"
#include <string>

namespace openmsx {

/** Thrown when a subsystem initialisation fails.
  * For example: opening video surface, opening audio output etc.
  */
class InitException: public MSXException
{
public:
	InitException(const std::string& message_)
		: MSXException(message_) { }
	virtual ~InitException() { }
};

} // namespace openmsx

#endif // __INITEXCEPTION_HH__
