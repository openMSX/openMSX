// $Id$

#include "Command.hh"
#include "CommandResult.hh"

namespace openmsx {

void SimpleCommand::execute(const vector<string>& tokens, CommandResult& result)
{
	result.setString(execute(tokens));
}

} // namespace openmsx
