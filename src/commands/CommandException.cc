// $Id$

#include "CommandException.hh"

namespace openmsx {

CommandException::CommandException(const std::string& message)
	: MSXException(message)
{
}

CommandException::CommandException(const char* message)
	: MSXException(message)
{
}

SyntaxError::SyntaxError()
	: CommandException("Syntax error")
{
}

} // namespace openmsx

