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
#include "SettingListener.hh"
#include "MSXConfig.hh"

namespace openmsx {

class CommandConsole : public Console, private EventListener,
                       private SettingListener
{
public:
	/** Get singleton console instance.
	  */
	static CommandConsole *instance();
	void registerDebugger();

	/** Prints a string on the console.
	  */
	void printFast(const string &text);
	void printFlush();
	void print(const string &text);
	
	virtual unsigned getScrollBack() const;
	virtual const string &getLine(unsigned line) const;
	virtual bool isVisible() const;
	virtual void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const;
	virtual void setCursorPosition(unsigned xPosition, unsigned yPosition);
	virtual void setConsoleDimensions(unsigned columns, unsigned rows);
	virtual const string& getId() const;

private:
	static const int LINESHISTORY = 100;
	
	CommandConsole();
	virtual ~CommandConsole();
	virtual bool signalEvent(const SDL_Event& event) throw();
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
	void putCommandHistory(const string &command);
	void newLineConsole(const string &line);
	void putPrompt();
	void resetScrollBack();
	
	void combineLines(CircularBuffer<string, LINESHISTORY> &buffer,
			  CircularBuffer<bool, LINESHISTORY> &overflows,
			  bool fromTop = false);
	void splitLines();
	void loadHistory();
	void saveHistory();
	
	void update(const SettingLeafNode* setting) throw();

	BooleanSetting consoleSetting;
	unsigned int maxHistory;
	string editLine;
	// Are double commands allowed ?
	bool removeDoubles;
	CircularBuffer<string, LINESHISTORY> lines;
	CircularBuffer<bool, LINESHISTORY> lineOverflows;
	list<string> history;
	list<string>::iterator commandScrollBack;
	// saves Current Command to enable command recall
	string currentLine;
	unsigned consoleScrollBack;
	unsigned cursorLocationX;
	unsigned cursorLocationY;
	unsigned cursorPosition; // position within the current command
	unsigned consoleColumns;
	unsigned consoleRows;
};

} // namespace openmsx

#endif
