// $Id$

#include "Command.hh"
#include "Console.hh"


void Command::print(const std::string &message) const
{
	Console::instance()->print(message);
}

