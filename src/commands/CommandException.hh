// $Id$

#ifndef __COMMANDEXCEPTION_HH__
#define __COMMANDEXCEPTION_HH__

#include "MSXException.hh"

namespace openmsx {

class CommandException : public MSXException {
public:
	CommandException(const std::string& desc);
	virtual ~CommandException();
};

class SyntaxError : public CommandException {
public:
	SyntaxError();
	virtual ~SyntaxError();
};

} // namespace openmsx

#endif
