// $Id$

#include "Console.hh"
#include "OSDConsoleRenderer.hh"

namespace openmsx {

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
	for (list<ConsoleRenderer*>::iterator it = renderers.begin();
	     it != renderers.end();
	     ++it) {
		(*it)->updateConsole();
	}
}

} // namespace openmsx
