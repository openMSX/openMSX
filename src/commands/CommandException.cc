// $Id$

#include "CommandException.hh"


namespace openmsx {

CommandException::CommandException(const std::string& desc)
	: MSXException(desc)
{
}

SyntaxError::SyntaxError()
	: CommandException("Syntax error")
{
}

} // namespace openmsx

