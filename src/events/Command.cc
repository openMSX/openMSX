// $Id$

#include "Command.hh"
#include "CommandConsole.hh"


void Command::print(const std::string &message) const
{
	CommandConsole::instance()->print(message);
}
