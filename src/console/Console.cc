// $Id$

#include "Console.hh"
#include "OSDConsoleRenderer.hh"

Console::Console ()
{
}

Console::~Console()
{
}

void Console::registerConsole(ConsoleRenderer *console)
{
	renderers.push_back(console);
}

void Console::unregisterConsole(ConsoleRenderer *console)
{
	renderers.remove(console);
}

void Console::updateConsole()
{
	for (std::list<ConsoleRenderer*>::iterator it = renderers.begin();
	     it != renderers.end();
	     it++) {
		(*it)->updateConsole();
	}
}
