// $Id$

#include "Command.hh"
#include "CommandConsole.hh"


namespace openmsx {

void Command::print(const string &message) const
{
	CommandConsole::instance()->print(message);
}

} // namespace openmsx
