// $Id$

#include "Command.hh"
#include "CommandConsole.hh"


void Command::print(const string &message) const
{
	CommandConsole::instance()->print(message);
}
