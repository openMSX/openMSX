// $Id$

#ifndef __SDLINTERACTIVECONSOLE_HH__
#define __SDLINTERACTIVECONSOLE_HH__

#include "EventListener.hh"
#include "InteractiveConsole.hh"
#include "Command.hh"

class FileContext;


class SDLInteractiveConsole : public InteractiveConsole, private EventListener
{
	public:
		SDLInteractiveConsole();
		virtual ~SDLInteractiveConsole();

	protected:
		bool isVisible;
		std::string fontName;
		std::string backgroundName;
		const FileContext* context;

	private:
		virtual bool signalEvent(SDL_Event &event, const EmuTime &time);
		
		class ConsoleCmd : public Command {
			public:
				ConsoleCmd(SDLInteractiveConsole *cons);
				virtual void execute(const std::vector<std::string> &tokens,
				                     const EmuTime &time);
				virtual void help(const std::vector<std::string> &tokens) const;
			private:
				SDLInteractiveConsole *console;
		};
		friend class ConsoleCmd;
		ConsoleCmd consoleCmd;
};

#endif
