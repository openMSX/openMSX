// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__
#include <SDL/SDL.h>
#include "ConsoleSource/CON_console.h"
#include "openmsx.hh"
#include "EventDistributor.hh"
#include "HotKey.hh"

class ConsoleInterface;

class Console : private EventListener , private HotKeyListener
{
	public:
		virtual ~Console();
		static Console* instance();

		// objects that want to accept commands from the console use
		// the bool returns indicate failure or succes.
		bool registerCommand(ConsoleInterface registeredObject,char *command);
		bool unRegisterCommand(ConsoleInterface registeredObject,char *command);

		// SDL dependend stuff
		// TODO: make SDL independend if possible

		// The render that instanciates the SDL_Screen needs to call this function
		/**
		 * Every MSXMemoryMapper must registers its size.
		 * This is used to influence the result returned in convert().
		 */
		void hookUpConsole(SDL_Surface *Screen);
		void unHookConsole(SDL_Surface *Screen);

		// Routine draws console if needed
		void drawConsole();
		void signalHotKey(SDLKey key);
		// Needs to be called to handle key inputs
		void signalEvent(SDL_Event &event);

	private:
		SDL_Rect Con_rect;
		ConsoleInformation *ConsoleStruct;
		bool isHookedUp;
		bool isVisible;
		static Console* oneInstance;
};

#endif
