// $Id$

#include "openmsx.hh"
#include "Console.hh"
#include "ConsoleCommand.hh"
#include <cassert>

Console *Console::oneInstance;

Console::~Console()
{
	PRT_DEBUG("Destroying a Console object");
}

Console *Console::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new Console();
		oneInstance->Con_rect.x = 20;
		oneInstance->Con_rect.y = 300;
		oneInstance->Con_rect.w = 600;
		oneInstance->Con_rect.h = 180;
		oneInstance->isHookedUp = false;
		oneInstance->isVisible = false;
		//oneInstance->registeredCounter = 0;
	}
	return oneInstance;
}

void Console::registerCommand(ConsoleCommand &registeredObject, char *command)
{
	CON_AddCommand(&registeredObject, command);
}

void Console::unRegisterCommand(ConsoleCommand &registeredObject, char *command)
{
	assert(false);	// unimplemented
}

void Console::printOnConsole(std::string text)
{
	int end = 0;
	bool more = true;
	while (more) {
		int start = end;
		end = text.find('\n', start);
		if (end == -1) {
			more = false;
			end = text.length();
		}
		char line[end - start + 1];
		text.copy(line, sizeof(line), start);
		line[sizeof(line) - 1] = '\0';
		CON_Out(ConsoleStruct, line);
		end++; // skip newline
	}
}

// SDL dependent stuff
// TODO: make SDL independent if possible

void Console::hookUpConsole(SDL_Surface *Screen)
{
	ConsoleStruct = CON_Init("ConsoleFont.bmp", Screen, 100, Con_rect);
	if (ConsoleStruct) {
		//CON_Out(ConsoleStruct, "test text in console");
		//CON_Out(ConsoleStruct, "nothing usefull yet!");
		CON_Alpha(ConsoleStruct, 200);
		CON_Topmost(ConsoleStruct);
		SDL_EnableUNICODE(1);
		isHookedUp = true;
		//isVisible=true;
		//now that ConsoleStruct exists we can register listener
		// otherwise there is nothing to deliver the events to.
		// and the hotkey could enable a non existing console

		/*
		If register as synclistener it is automatically also registered as asynclistener and while
		therefore get double events delivered !!
		EventDistributor::instance()->registerSyncListener(SDL_KEYDOWN, oneInstance);
		*/
		EventDistributor::instance()->registerAsyncListener(SDL_KEYDOWN, oneInstance);
		HotKey::instance()->registerAsyncHotKey(SDLK_F10, oneInstance);
	}

}

void Console::unHookConsole(SDL_Surface *Screen)
{
	assert(false);
}

// If console is visible
void Console::drawConsole()
{
   if (isVisible) CON_DrawConsole(ConsoleStruct);
}

// Note: this runs in a different thread
void Console::signalHotKey(SDLKey key) {
	if (key == SDLK_F10) {
		isVisible = !isVisible;
		//CON_Out(ConsoleStruct,"hotkey pressed");
		//CON_Out(ConsoleStruct, isVisible ? "is visible" : "is not visible");
	} else {
		assert(false);
	}
}

void Console::signalEvent(SDL_Event &event)
{
	if (isVisible){
		CON_Events(&event);
		//CON_Out(ConsoleStruct,"got event");
	}
}
