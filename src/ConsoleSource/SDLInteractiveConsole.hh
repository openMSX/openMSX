// $Id$

#ifndef __SDLINTERACTIVECONSOLE_HH__
#define __SDLINTERACTIVECONSOLE_HH__

#include "EventListener.hh"
#include "InteractiveConsole.hh"
#include "Command.hh"


class SDLInteractiveConsole : public InteractiveConsole, private EventListener
{
	public:
		SDLInteractiveConsole();
		virtual ~SDLInteractiveConsole();

	protected:
		bool isVisible;

	private:
		virtual bool signalEvent(SDL_Event &event);
		
		class ConsoleCmd : public Command {
			public:
				ConsoleCmd(SDLInteractiveConsole *cons);
				virtual void execute(const std::vector<std::string> &tokens);
				virtual void help   (const std::vector<std::string> &tokens);
			private:
				SDLInteractiveConsole *console;
		};
		friend class ConsoleCmd;
		ConsoleCmd consoleCmd;
};

#endif
