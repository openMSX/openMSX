// $Id$

#ifndef __COMMANDCONSOLE_HH__
#define __COMMANDCONSOLE_HH__

#include <list>
#include <string>
#include "Console.hh"
#include "EventListener.hh"
#include "CircularBuffer.hh"
#include "BooleanSetting.hh"
#include "SettingListener.hh"


namespace openmsx {

class EventDistributor;
class SettingsConfig;
class CommandController;
class CliCommOutput;
class SettingsManager;
class InputEventGenerator;

class CommandConsole : public Console, private EventListener,
                       private SettingListener
{
public:
	static CommandConsole& instance();
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
	virtual bool signalEvent(const Event& event) throw();
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
		bool fromTop = false );
	void splitLines();
	void loadHistory();
	void saveHistory();

	void update(const SettingLeafNode* setting) throw();

	BooleanSetting consoleSetting;
	unsigned int maxHistory;
	string editLine;
	string commandBuffer;
	string prompt;
	/** Are double commands allowed? */
	bool removeDoubles;
	CircularBuffer<string, LINESHISTORY> lines;
	CircularBuffer<bool, LINESHISTORY> lineOverflows;
	list<string> history;
	list<string>::iterator commandScrollBack;
	/** Saves Current Command to enable command recall. */
	string currentLine;
	unsigned consoleScrollBack;
	unsigned cursorLocationX;
	unsigned cursorLocationY;
	/** Position within the current command. */
	unsigned cursorPosition;
	unsigned consoleColumns;
	unsigned consoleRows;

	EventDistributor& eventDistributor;
	SettingsConfig& settingsConfig;
	CommandController& commandController;
	CliCommOutput& output;
	SettingsManager& settingsManager;
	InputEventGenerator& inputEventGenerator;
};

} // namespace openmsx

#endif // __COMMANDCONSOLE_HH__
