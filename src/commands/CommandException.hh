// $Id$

#ifndef COMMANDEXCEPTION_HH
#define COMMANDEXCEPTION_HH

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
