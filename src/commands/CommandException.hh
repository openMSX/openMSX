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

class SyntaxError : public CommandException {
public:
	SyntaxError() : CommandException("Syntax error") {}
};

} // namespace openmsx

#endif
