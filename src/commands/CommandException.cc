#include "CommandException.hh"

namespace openmsx {

CommandException::CommandException(string_ref message)
	: MSXException(message)
{
}

SyntaxError::SyntaxError()
	: CommandException("Syntax error")
{
}

} // namespace openmsx
