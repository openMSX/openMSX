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
		/** Get singleton console instance.
		  */
		static Console *instance();
	
		/** Prints a string on the console.
		  */
		void print(const std::string &text);
		
		/** Add a renderer for this console.
		  */
		void registerConsole(ConsoleRenderer *console);
		
		/** Remove a renderer for this console.
		  */
		void unregisterConsole(ConsoleRenderer *console);
		int getScrollBack();
		const std::string& getLine(int line);
		bool isVisible();
		int getCursorPosition(){return cursorPosition;};
		int setCursorPosition(int position);
		void setConsoleColumns(int columns){consoleColumns=columns;};

	private:
		Console();
		virtual ~Console();

		virtual bool signalEvent(SDL_Event &event);
			
		void tabCompletion();
		void commandExecute();
		void scrollUp();
		void scrollDown();
		void prevCommand();
		void nextCommand();
		void clearCommand();
		void backspace();
		void delete_key();
		void normalKey(char chr);
		void putCommandHistory(const std::string &command);
		void newLineConsole(const std::string &line);
		void putPrompt();
		void updateConsole();
		void resetScrollBack();
		class ConsoleSetting : public BooleanSetting
		{
			public:
				ConsoleSetting(Console *console);
			protected:
				virtual bool checkUpdate(bool newValue);
			private:
				Console *console;
		} consoleSetting;
		friend class ConsoleSetting;
		
		unsigned int maxHistory;
		std::list<ConsoleRenderer*> renderers;
		CircularBuffer<std::string, 100> lines;
// 		Are double commands allowed ?
		bool removeDoubles;
//		CircularBuffer<std::string, 25> history;
		std::list<std::string> history;
		std::list<std::string>::iterator commandScrollBack;
		// saves Current Command to enable command recall
		std::string currentLine;
		int consoleScrollBack;
//		int commandScrollBack;
		unsigned int cursorPosition;
		int consoleColumns;
};

#endif
