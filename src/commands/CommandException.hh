// $Id$

#ifndef __COMMANDEXCEPTION_HH__
#define __COMMANDEXCEPTION_HH__

#include "MSXException.hh"

namespace openmsx {

class CommandException : public MSXException {
public:
	CommandException(const string& desc)
		: MSXException(desc) {}
};

} // namespace openmsx

#endif
