#ifndef COMMANDCONSOLE_HH
#define COMMANDCONSOLE_HH

#include "EventListener.hh"
#include "InterpreterOutput.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "gl_vec.hh"
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
	[[nodiscard]] size_t numChars() const;
	/** Get the total string, ignoring color differences. */
	[[nodiscard]] const std::string& str() const { return line; }

	/** Get the number of different chunks. Each chunk is a a part of the
	  * line that has the same color. */
	[[nodiscard]] size_t numChunks() const { return chunks.size(); }
	/** Get the color for the i-th chunk. */
	[[nodiscard]] uint32_t chunkColor(size_t i) const;
	/** Get the text for the i-th chunk. */
	[[nodiscard]] std::string_view chunkText(size_t i) const;

	[[nodiscard]] const auto& getChunks() const { return chunks; }

private:
	std::string line;
	struct Chunk {
		uint32_t rgb;
		std::string_view::size_type pos;
	};
	std::vector<Chunk> chunks;
};


class CommandConsole final : private EventListener
                           , private InterpreterOutput
{
public:
	CommandConsole(GlobalCommandController& commandController,
	               EventDistributor& eventDistributor,
	               Display& display);
	~CommandConsole();

	[[nodiscard]] BooleanSetting& getConsoleSetting() { return consoleSetting; }

	[[nodiscard]] unsigned getScrollBack() const { return consoleScrollBack; }
	[[nodiscard]] gl::ivec2 getCursorPosition() const;

	void setColumns(unsigned columns_) { columns = columns_; }
	[[nodiscard]] unsigned getColumns() const { return columns; }
	void setRows(unsigned rows_) { rows = rows_; }
	[[nodiscard]] unsigned getRows() const { return rows; }

	[[nodiscard]] const auto& getLines() const { return lines; }

private:
	// InterpreterOutput
	void output(std::string_view text) override;
	[[nodiscard]] unsigned getOutputColumns() const override;

	// EventListener
	int signalEvent(const Event& event) override;

	bool handleEvent(const KeyEvent& keyEvent);
	void tabCompletion();
	void commandExecute();
	void scroll(int delta);
	void gotoStartOfWord();
	void deleteToStartOfWord();
	void gotoEndOfWord();
	void deleteToEndOfWord();
	void prevCommand();
	void nextCommand();
	void clearCommand();
	void clearHistory();
	void backspace();
	void delete_key();
	void normalKey(uint32_t chr);
	void putCommandHistory(const std::string& command);
	void newLineConsole(std::string line);
	void newLineConsole(ConsoleLine line);
	void putPrompt();
	void resetScrollBack();
	void paste();
	[[nodiscard]] ConsoleLine highLight(std::string_view line);

	/** Prints a string on the console.
	  */
	void print(std::string_view text, unsigned rgb = 0xffffff);

	void loadHistory();
	void saveHistory();

private:
	GlobalCommandController& commandController;
	EventDistributor& eventDistributor;
	Display& display;
	BooleanSetting consoleSetting;
	IntegerSetting historySizeSetting;
	BooleanSetting removeDoublesSetting;

	static constexpr int LINESHISTORY = 1000;
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
	bool executingCommand = false;
};

} // namespace openmsx

#endif
