// $Id$

#include "ConsoleManager.hh"
#include "Console.hh"


ConsoleManager* ConsoleManager::instance()
{
	static ConsoleManager* oneInstance = NULL;
	if (oneInstance == NULL) {
		oneInstance = new ConsoleManager();
	}
	return oneInstance;
}

void ConsoleManager::print(const std::string &text)
{
	std::list<Console*>::iterator i;
	for (i = consoles.begin(); i != consoles.end(); i++) {
		(*i)->print(text);
	}
}

void ConsoleManager::registerConsole(Console *console)
{
	consoles.push_back(console);
}
void ConsoleManager::unregisterConsole(Console *console)
{
	consoles.remove(console);
}
