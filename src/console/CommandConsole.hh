// $Id$

#ifndef __COMMANDCONSOLE_HH__
#define __COMMANDCONSOLE_HH__

#include <list>
#include <string>
#include <fstream>
#include "Console.hh"
#include "EventListener.hh"
#include "CircularBuffer.hh"
#include "Settings.hh"
#include "MSXConfig.hh"

class CommandConsole : public Console, private EventListener, private SettingListener
{
	public:
		/** Get singleton console instance.
		  */
		static CommandConsole *instance();
	
		/** Prints a string on the console.
		  */
		void printFast(const std::string &text);
		void printFlush();
		void print(const std::string &text);
		
		int getScrollBack();
		const std::string &getLine(int line);
		bool isVisible();
		void getCursorPosition(int *xPosition, int *yPosition);
		void setCursorPosition(int xPosition, int yPosition);
		void setCursorPosition(CursorXY pos);
		void setConsoleDimensions(int columns, int rows);

	private:
		static const int LINESHISTORY = 100;
		
		CommandConsole();
		virtual ~CommandConsole();
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
		void resetScrollBack();
		
		void combineLines(CircularBuffer<std::string, LINESHISTORY> &buffer,
		                  CircularBuffer<bool, LINESHISTORY> &overflows,
		                  bool fromTop = false);
		void splitLines();
		void loadHistory();
		void saveHistory();
		
		void notify(Setting *setting);
		
		BooleanSetting consoleSetting;
		unsigned int maxHistory;
		std::string editLine;
		// Are double commands allowed ?
		bool removeDoubles;
		CircularBuffer<std::string, 100> lines;
		CircularBuffer<bool, 100> lineOverflows;
		std::list<std::string> history;
		std::list<std::string>::iterator commandScrollBack;
		// saves Current Command to enable command recall
		std::string currentLine;
		int consoleScrollBack;
		struct CursorXY cursorLocation; // full cursorcoordinates
		int cursorPosition; // position within the current command
		int consoleColumns;
		int consoleRows;
};

#endif
