// $Id$

#ifndef __INTERACTIVECONSOLE_HH__
#define __INTERACTIVECONSOLE_HH__

#include "Console.hh"
#include "CircularBuffer.hh"

class EmuTime;


class InteractiveConsole : public Console
{
	public:
		InteractiveConsole();
		virtual ~InteractiveConsole();

		void print(const std::string &text);
		
		/**
		 * This method redraws console, it must be called every frame
		 */
		virtual void drawConsole() = 0;
		
	protected:
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
		
		virtual void updateConsole() {}
		
		
		/** This is what the prompt looks like
		  */
		static const std::string PROMPT;
		
		/** List of all the past lines.
		  */
		CircularBuffer<std::string, 100> lines;

		/** List of all the past commands.
		  */
		CircularBuffer<std::string, 25> history;

		/** How much the users scrolled back in the console.
		  */
		int consoleScrollBack;

		/** How much the users scrolled back in the command lines.
		  */
		int commandScrollBack;
};

#endif
