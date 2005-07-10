// $Id$

#ifndef COMMANDCONSOLE_HH
#define COMMANDCONSOLE_HH

#include <list>
#include <string>
#include "Console.hh"
#include "CircularBuffer.hh"
#include "openmsx.hh"

namespace openmsx {

class SettingsConfig;
class CommandController;
class CliComm;

class CommandConsole : public Console
{
public:
	static CommandConsole& instance();

	/** Prints a string on the console.
	  */
	void print(std::string text);

	virtual unsigned getScrollBack() const;
	virtual std::string getLine(unsigned line) const;
	virtual void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const;

private:
	static const int LINESHISTORY = 1000;

	CommandConsole();
	virtual ~CommandConsole();
	virtual bool signalEvent(const UserInputEvent& event);
	void tabCompletion();
	void commandExecute();
	void scroll(int delta);
	void prevCommand();
	void nextCommand();
	void clearCommand();
	void backspace();
	void delete_key();
	void normalKey(word chr);
	void putCommandHistory(const std::string& command);
	void newLineConsole(const std::string& line);
	void putPrompt();
	void resetScrollBack();

	void loadHistory();
	void saveHistory();

	unsigned maxHistory;
	std::string commandBuffer;
	std::string prompt;
	/** Are double commands allowed? */
	bool removeDoubles;
	CircularBuffer<std::string, LINESHISTORY> lines;
	std::list<std::string> history;
	std::list<std::string>::iterator commandScrollBack;
	/** Saves Current Command to enable command recall. */
	std::string currentLine;
	int consoleScrollBack;
	/** Position within the current command. */
	unsigned cursorPosition;

	SettingsConfig& settingsConfig;
	CommandController& commandController;
	CliComm& output;
};

} // namespace openmsx

#endif
