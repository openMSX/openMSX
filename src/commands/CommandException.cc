#include "CommandException.hh"

namespace openmsx {

SyntaxError::SyntaxError()
	: CommandException("Syntax error")
{
}

} // namespace openmsx
