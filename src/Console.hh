// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__
#include <SDL/SDL.h>
#include <string>
#include "ConsoleSource/CON_console.hh"
#include "ConsoleSource/CON_consolecommands.hh"
#include "openmsx.hh"
#include "EventDistributor.hh"
#include "HotKey.hh"

class ConsoleCommand;

class Console : private EventListener , private HotKeyListener
{
	public:
		virtual ~Console();
		static Console* instance();

		// objects that want to accept commands from the console use
		void registerCommand(ConsoleCommand &registeredObject, char *command);
		void unRegisterCommand(ConsoleCommand &registeredObject, char *command);
		void printOnConsole(std::string text);
		// SDL dependend stuff
		// TODO: make SDL independend if possible

		// The render that instanciates the SDL_Screen needs to call this function
		/**
		 * Every SDLRenderer must registers its SDL_screen.
		 * This is used to influence the result of the Renderer
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
