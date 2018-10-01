#ifndef COMMANDEXCEPTION_HH
#define COMMANDEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class CommandException : public MSXException
{
public:
	using MSXException::MSXException;
};

class SyntaxError final : public CommandException
{
public:
	SyntaxError();
};

} // namespace openmsx

#endif
