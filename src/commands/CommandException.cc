// $Id$

#include "CommandException.hh"


namespace openmsx {

CommandException::CommandException(const string& desc)
	: MSXException(desc)
{
}

CommandException::~CommandException()
{
}

SyntaxError::SyntaxError()
	: CommandException("Syntax error")
{
}

SyntaxError::~SyntaxError()
{
}

} // namespace openmsx

