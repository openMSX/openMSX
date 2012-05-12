// $Id$

#ifndef COMMANDCONSOLE_HH
#define COMMANDCONSOLE_HH

#include "EventListener.hh"
#include "InterpreterOutput.hh"
#include "CircularBuffer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <list>
#include <string>
#include <memory>

namespace openmsx {

class GlobalCommandController;
class EventDistributor;
class KeyEvent;
class BooleanSetting;
class IntegerSetting;
class Display;

class CommandConsole : private EventListener,
                       private InterpreterOutput, private noncopyable
{
public:
	CommandConsole(GlobalCommandController& commandController,
	               EventDistributor& eventDistributor,
	               Display& display);
	virtual ~CommandConsole();

	BooleanSetting& getConsoleSetting();

	unsigned getScrollBack() const;
	std::string getLine(unsigned line) const;
	void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const;

	void setColumns(unsigned columns);
	unsigned getColumns() const;
	void setRows(unsigned rows);
	unsigned getRows() const;

private:
	// InterpreterOutput
	virtual void output(const std::string& text);
	virtual unsigned getOutputColumns() const;

	// EventListener
	virtual int signalEvent(const shared_ptr<const Event>& event);

	bool handleEvent(const KeyEvent& keyEvent);
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

	/** Prints a string on the console.
	  */
	void print(std::string text);

	void loadHistory();
	void saveHistory();

	GlobalCommandController& commandController;
	EventDistributor& eventDistributor;
	Display& display;
	std::auto_ptr<BooleanSetting> consoleSetting;
	std::auto_ptr<IntegerSetting> historySizeSetting;
	std::auto_ptr<BooleanSetting> removeDoublesSetting;

	static const int LINESHISTORY = 1000;
	CircularBuffer<std::string, LINESHISTORY> lines;
	std::string commandBuffer;
	std::string prompt;
	/** Saves Current Command to enable command recall. */
	std::string currentLine;
	typedef std::list<std::string> History;
	History history;
	History::const_iterator commandScrollBack;
	unsigned columns;
	unsigned rows;
	int consoleScrollBack;
	/** Position within the current command. */
	unsigned cursorPosition;
	bool executingCommand;
};

} // namespace openmsx

#endif
