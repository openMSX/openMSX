#include "CommandException.hh"

namespace openmsx {

CommandException::CommandException(string_ref message_)
	: MSXException(message_)
{
}

SyntaxError::SyntaxError()
	: CommandException("Syntax error")
{
}

} // namespace openmsx
