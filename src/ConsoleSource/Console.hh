// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#include <list>
#include <string>
#include "EventListener.hh"
#include "CircularBuffer.hh"
#include "Settings.hh"

class ConsoleRenderer;


class Console : private EventListener
{
	public:
		static Console* instance();
	
		/**
		 * Prints a string on the console
		 */
		void print(const std::string &text);
		
		/**
		 * (Un)register a ConsoleRenderer
		 */
		void registerConsole(ConsoleRenderer* console);
		void unregisterConsole(ConsoleRenderer* console);

		int getScrollBack();
		const std::string& getLine(int line);
		bool isVisible();
		
	private:
		Console();
		virtual ~Console();

		virtual bool signalEvent(SDL_Event &event, const EmuTime &time);

		void tabCompletion();
		void commandExecute(const EmuTime &time);
		void scrollUp();
		void scrollDown();
		void prevCommand();
		void nextCommand();
		void backspace();
		void normalKey(char chr);
		
		void putCommandHistory(const std::string &command);
		void newLineConsole(const std::string &line);
		void putPrompt();
		void updateConsole();

		class ConsoleSetting : public BooleanSetting
		{
			public:
				ConsoleSetting(Console *console);
			protected:
				virtual bool checkUpdate(bool newValue,
				                         const EmuTime &time);
			private:
				Console* console;
		} consoleSetting;
		
		std::list<ConsoleRenderer*> renderers;
		CircularBuffer<std::string, 100> lines;
		CircularBuffer<std::string, 25> history;
		int consoleScrollBack;
		int commandScrollBack;
};

#endif
