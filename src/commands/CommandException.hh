#ifndef COMMANDEXCEPTION_HH
#define COMMANDEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class CommandException : public MSXException
{
public:
	explicit CommandException(string_ref message);
};

class SyntaxError : public CommandException
{
public:
	SyntaxError();
};

} // namespace openmsx

#endif
