// $Id$

#ifndef COMMANDCONSOLE_HH
#define COMMANDCONSOLE_HH

#include "Console.hh"
#include "EventListener.hh"
#include "InterpreterOutput.hh"
#include "CircularBuffer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <list>
#include <string>

namespace openmsx {

class CommandController;
class EventDistributor;
class KeyEvent;
class BooleanSetting;
class Display;

class CommandConsole : public Console, private EventListener,
                       private InterpreterOutput, private noncopyable
{
public:
	CommandConsole(CommandController& commandController,
	               EventDistributor& eventDistributor,
	               Display& display);
	virtual ~CommandConsole();

	/** Prints a string on the console.
	  */
	virtual void print(std::string text);

	virtual unsigned getScrollBack() const;
	virtual std::string getLine(unsigned line) const;
	virtual void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const;

private:
	// InterpreterOutput
	virtual void output(const std::string& text);

	static const int LINESHISTORY = 1000;

	virtual bool signalEvent(shared_ptr<const Event> event);
	void handleEvent(const KeyEvent& keyEvent);
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

	CommandController& commandController;
	EventDistributor& eventDistributor;
	Display& display;
	BooleanSetting& consoleSetting;
};

} // namespace openmsx

#endif
