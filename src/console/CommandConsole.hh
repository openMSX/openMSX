// $Id$

#ifndef __COMMANDCONSOLE_HH__
#define __COMMANDCONSOLE_HH__

#include <list>
#include <string>
#include "Console.hh"
#include "CircularBuffer.hh"
#include "openmsx.hh"

namespace openmsx {

class SettingsConfig;
class CommandController;
class CliCommOutput;

class CommandConsole : public Console
{
public:
	static CommandConsole& instance();

	/** Prints a string on the console.
	  */
	void print(const std::string &text);

	virtual unsigned getScrollBack() const;
	virtual const std::string &getLine(unsigned line) const;
	virtual void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const;
	virtual void setCursorPosition(unsigned xPosition, unsigned yPosition);
	virtual void setConsoleDimensions(unsigned columns, unsigned rows);

private:
	static const int LINESHISTORY = 1000;

	CommandConsole();
	virtual ~CommandConsole();
	virtual bool signalEvent(const Event& event);
	void tabCompletion();
	void commandExecute();
	void scroll(int delta);
	void prevCommand();
	void nextCommand();
	void clearCommand();
	void backspace();
	void delete_key();
	void normalKey(word chr);
	void putCommandHistory(const std::string &command);
	void newLineConsole(const std::string &line);
	void putPrompt();
	void resetScrollBack();

	void combineLines(CircularBuffer<std::string, LINESHISTORY> &buffer,
		CircularBuffer<bool, LINESHISTORY> &overflows,
		bool fromTop = false );
	void splitLines();
	void loadHistory();
	void saveHistory();

	unsigned int maxHistory;
	std::string editLine;
	std::string commandBuffer;
	std::string prompt;
	/** Are double commands allowed? */
	bool removeDoubles;
	CircularBuffer<std::string, LINESHISTORY> lines;
	CircularBuffer<bool, LINESHISTORY> lineOverflows;
	std::list<std::string> history;
	std::list<std::string>::iterator commandScrollBack;
	/** Saves Current Command to enable command recall. */
	std::string currentLine;
	int consoleScrollBack;
	unsigned cursorLocationX;
	unsigned cursorLocationY;
	/** Position within the current command. */
	unsigned cursorPosition;
	unsigned consoleColumns;
	unsigned consoleRows;

	SettingsConfig& settingsConfig;
	CommandController& commandController;
	CliCommOutput& output;
};

} // namespace openmsx

#endif // __COMMANDCONSOLE_HH__
