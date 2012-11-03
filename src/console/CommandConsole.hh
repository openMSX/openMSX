// $Id$

#ifndef COMMANDCONSOLE_HH
#define COMMANDCONSOLE_HH

#include "EventListener.hh"
#include "InterpreterOutput.hh"
#include "CircularBuffer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include "string_ref.hh"
#include <list>
#include <memory>
#include <vector>

namespace openmsx {

class GlobalCommandController;
class EventDistributor;
class KeyEvent;
class BooleanSetting;
class IntegerSetting;
class Display;

/** This class represents a single text line in the console.
  * The line can have several chunks with different colors. */
class ConsoleLine
{
public:
	/** Construct empty line. */
	ConsoleLine();
	/** Construct line with a single color (by default white). */
	explicit ConsoleLine(string_ref line, unsigned rgb = 0xffffff);

	/** Append a chunk with a (different) color. This is currently the
	  * only way to construct a multi-colored line/ */
	void addChunk(string_ref text, unsigned rgb);

	/** Get the number of UTF8 characters in this line. So multi-byte
	  * characters are counted as a single character. */
	unsigned numChars() const;
	/** Get the total string, ignoring color differences. */
	const std::string& str() const;

	/** Get the number of different chunks. Each chunk is a a part of the
	  * line that has the same color. */
	unsigned numChunks() const;
	/** Get the color for the i-th chunk. */
	unsigned chunkColor(unsigned i) const;
	/** Get the text for the i-th chunk. */
	string_ref chunkText(unsigned i) const;

	/** Get a part of total line. The result keeps the same colors as this
	  * line. E.g. used to get part of (long) line that should be wrapped
	  * over multiple console lines.
	  * @param pos First character (multi-byte sequence counted as 1
	  *            character).
	  * @param len Length of the substring, also counted in characters
	  */
	ConsoleLine substr(unsigned pos, unsigned len) const;

private:
	std::string line;
	std::vector<std::pair<unsigned, string_ref::size_type> > chunks;
};


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
	ConsoleLine getLine(unsigned line) const;
	void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const;

	void setColumns(unsigned columns);
	unsigned getColumns() const;
	void setRows(unsigned rows);
	unsigned getRows() const;

private:
	// InterpreterOutput
	virtual void output(string_ref text);
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
	void newLineConsole(string_ref line);
	void newLineConsole(ConsoleLine line);
	void putPrompt();
	void resetScrollBack();
	ConsoleLine highLight(string_ref command);

	/** Prints a string on the console.
	  */
	void print(string_ref text, unsigned rgb = 0xffffff);

	void loadHistory();
	void saveHistory();

	GlobalCommandController& commandController;
	EventDistributor& eventDistributor;
	Display& display;
	std::unique_ptr<BooleanSetting> consoleSetting;
	std::unique_ptr<IntegerSetting> historySizeSetting;
	std::unique_ptr<BooleanSetting> removeDoublesSetting;

	static const int LINESHISTORY = 1000;
	CircularBuffer<ConsoleLine, LINESHISTORY> lines;
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
