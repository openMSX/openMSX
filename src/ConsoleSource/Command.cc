// $Id$

#include "Command.hh"
#include "ConsoleManager.hh"


void Command::print(const std::string &message) const
{
	ConsoleManager::instance()->print(message);
}

