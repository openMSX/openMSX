// $Id$

#include "Command.hh"
#include "ConsoleManager.hh"


void Command::print(const std::string &message)
{
	ConsoleManager::instance()->print(message);
}

