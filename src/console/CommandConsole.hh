#ifndef COMMANDCONSOLE_HH
#define COMMANDCONSOLE_HH

#include "EventListener.hh"
#include "InterpreterOutput.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "CircularBuffer.hh"
#include "circular_buffer.hh"
#include <string_view>
#include <vector>

namespace openmsx {

class GlobalCommandController;
class EventDistributor;
class KeyEvent;
class Display;

/** This class represents a single text line in the console.
  * The line can have several chunks with different colors. */
class ConsoleLine
{
public:
	/** Construct empty line. */
	ConsoleLine() = default;

	/** Construct line with a single color (by default white). */
	explicit ConsoleLine(std::string line, uint32_t rgb = 0xffffff);

	/** Append a chunk with a (different) color. This is currently the
	  * only way to construct a multi-colored line/ */
	void addChunk(std::string_view text, uint32_t rgb);

	/** Get the number of UTF8 characters in this line. So multi-byte
	  * characters are counted as a single character. */
	size_t numChars() const;
	/** Get the total string, ignoring color differences. */
	const std::string& str() const { return line; }

	/** Get the number of different chunks. Each chunk is a a part of the
	  * line that has the same color. */
	size_t numChunks() const { return chunks.size(); }
	/** Get the color for the i-th chunk. */
	uint32_t chunkColor(size_t i) const;
	/** Get the text for the i-th chunk. */
	std::string_view chunkText(size_t i) const;

	/** Get a part of total line. The result keeps the same colors as this
	  * line. E.g. used to get part of (long) line that should be wrapped
	  * over multiple console lines.
	  * @param pos First character (multi-byte sequence counted as 1
	  *            character).
	  * @param len Length of the substring, also counted in characters
	  */
	ConsoleLine substr(size_t pos, size_t len) const;

private:
	std::string line;
	std::vector<std::pair<uint32_t, std::string_view::size_type>> chunks; // [rgb, pos]
};


class CommandConsole final : private EventListener
                           , private InterpreterOutput
{
public:
	CommandConsole(GlobalCommandController& commandController,
	               EventDistributor& eventDistributor,
	               Display& display);
	~CommandConsole();

	BooleanSetting& getConsoleSetting() { return consoleSetting; }

	unsigned getScrollBack() const { return consoleScrollBack; }
	ConsoleLine getLine(unsigned line) const;
	void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const;

	void setColumns(unsigned columns_) { columns = columns_; }
	unsigned getColumns() const { return columns; }
	void setRows(unsigned rows_) { rows = rows_; }
	unsigned getRows() const { return rows; }

private:
	// InterpreterOutput
	void output(std::string_view text) override;
	unsigned getOutputColumns() const override;

	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;

	bool handleEvent(const KeyEvent& keyEvent);
	void tabCompletion();
	void commandExecute();
	void scroll(int delta);
	void prevCommand();
	void nextCommand();
	void clearCommand();
	void backspace();
	void delete_key();
	void normalKey(uint32_t chr);
	void putCommandHistory(const std::string& command);
	void newLineConsole(std::string line);
	void newLineConsole(ConsoleLine line);
	void putPrompt();
	void resetScrollBack();
	ConsoleLine highLight(std::string_view line);

	/** Prints a string on the console.
	  */
	void print(std::string_view text, unsigned rgb = 0xffffff);

	void loadHistory();
	void saveHistory();

	GlobalCommandController& commandController;
	EventDistributor& eventDistributor;
	Display& display;
	BooleanSetting consoleSetting;
	IntegerSetting historySizeSetting;
	BooleanSetting removeDoublesSetting;

	static const int LINESHISTORY = 1000;
	CircularBuffer<ConsoleLine, LINESHISTORY> lines;
	std::string commandBuffer;
	std::string prompt;
	/** Saves Current Command to enable command recall. */
	std::string currentLine;
	circular_buffer<std::string> history;
	unsigned commandScrollBack;
	unsigned columns;
	unsigned rows;
	int consoleScrollBack;
	/** Position within the current command. */
	unsigned cursorPosition;
	bool executingCommand;
};

} // namespace openmsx

#endif
