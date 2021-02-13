#include "CommandConsole.hh"
#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "Completer.hh"
#include "Interpreter.hh"
#include "Keys.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "CliComm.hh"
#include "Event.hh"
#include "Display.hh"
#include "VideoSystem.hh"
#include "EventDistributor.hh"
#include "Version.hh"
#include "unreachable.hh"
#include "utf8_unchecked.hh"
#include "StringOp.hh"
#include "ScopedAssign.hh"
#include "view.hh"
#include "xrange.hh"
#include <algorithm>
#include <fstream>
#include <cassert>

using std::min;
using std::max;
using std::string;
using std::string_view;

namespace openmsx {

// class ConsoleLine

ConsoleLine::ConsoleLine(string line_, uint32_t rgb)
	: line(std::move(line_))
	, chunks(1, {rgb, 0})
{
}

void ConsoleLine::addChunk(string_view text, uint32_t rgb)
{
	chunks.emplace_back(Chunk{rgb, line.size()});
	line.append(text.data(), text.size());
}

size_t ConsoleLine::numChars() const
{
	return utf8::unchecked::size(line);
}

uint32_t ConsoleLine::chunkColor(size_t i) const
{
	assert(i < chunks.size());
	return chunks[i].rgb;
}

string_view ConsoleLine::chunkText(size_t i) const
{
	assert(i < chunks.size());
	auto pos = chunks[i].pos;
	auto len = ((i + 1) == chunks.size())
	         ? string_view::npos
	         : chunks[i + 1].pos - pos;
	return string_view(line).substr(pos, len);
}

// class CommandConsole

constexpr const char* const PROMPT_NEW  = "> ";
constexpr const char* const PROMPT_CONT = "| ";
constexpr const char* const PROMPT_BUSY = "*busy*";

CommandConsole::CommandConsole(
		GlobalCommandController& commandController_,
		EventDistributor& eventDistributor_,
		Display& display_)
	: commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, display(display_)
	, consoleSetting(
		commandController, "console",
		"turns console display on/off", false, Setting::DONT_SAVE)
	, historySizeSetting(
		commandController, "console_history_size",
		"amount of commands kept in console history", 100, 0, 10000)
	, removeDoublesSetting(
		commandController, "console_remove_doubles",
		"don't add the command to history if it's the same as the previous one",
		true)
	, history(std::max(1, historySizeSetting.getInt()))
	, executingCommand(false)
{
	resetScrollBack();
	prompt = PROMPT_NEW;
	newLineConsole(prompt);
	loadHistory();
	putPrompt();
	Completer::setOutput(this);

	const auto& fullVersion = Version::full();
	print(fullVersion);
	print(string(fullVersion.size(), '-'));
	print("\n"
	      "General information about openMSX is available at "
	      "http://openmsx.org.\n"
	      "\n"
	      "Type 'help' to see a list of available commands "
	      "(use <PgUp>/<PgDn> to scroll).\n"
	      "Or read the Console Command Reference in the manual.\n"
	      "\n");

	commandController.getInterpreter().setOutput(this);
	eventDistributor.registerEventListener(
		EventType::KEY_DOWN, *this, EventDistributor::CONSOLE);
	// also listen to KEY_UP events, so that we can consume them
	eventDistributor.registerEventListener(
		EventType::KEY_UP, *this, EventDistributor::CONSOLE);
}

CommandConsole::~CommandConsole()
{
	eventDistributor.unregisterEventListener(EventType::KEY_DOWN, *this);
	eventDistributor.unregisterEventListener(EventType::KEY_UP, *this);
	commandController.getInterpreter().setOutput(nullptr);
	Completer::setOutput(nullptr);
}

void CommandConsole::saveHistory()
{
	try {
		std::ofstream outputfile;
		FileOperations::openofstream(outputfile,
		        userFileContext("console").resolveCreate("history.txt"));
		if (!outputfile) {
			throw FileException(
				"Error while saving the console history.");
		}
		for (auto& s : history) {
			outputfile << s << '\n';
		}
	} catch (FileException& e) {
		commandController.getCliComm().printWarning(e.getMessage());
	}
}

void CommandConsole::loadHistory()
{
	try {
		std::ifstream inputfile(
		        userFileContext("console").
				resolveCreate("history.txt").c_str());
		string line;
		while (inputfile) {
			getline(inputfile, line);
			putCommandHistory(line);
		}
	} catch (FileException&) {
		// Error while loading the console history, ignore
	}
}

gl::ivec2 CommandConsole::getCursorPosition() const
{
	int xPosition = cursorPosition % getColumns();
	auto num = lines[0].numChars() / getColumns();
	int yPosition = unsigned(num - (cursorPosition / getColumns()));
	return {xPosition, yPosition};
}

int CommandConsole::signalEvent(const Event& event) noexcept
{
	if (!consoleSetting.getBoolean()) return 0;

	// If the console is open then don't pass the event to the MSX
	// (whetever the (keyboard) event is). If the event has a meaning for
	// the console, then also don't pass the event to the hotkey system.
	// For example PgUp, PgDown are keys that have both a meaning in the
	// console and are used by standard key bindings.
	return visit(overloaded{
		[&](const KeyDownEvent& keyEvent) {
			if (!executingCommand) {
				if (handleEvent(keyEvent)) {
					// event was used
					display.repaintDelayed(40000); // 25fps
					return EventDistributor::HOTKEY; // block HOTKEY and MSX
				}
			} else {
				// For commands that take a long time to execute (e.g.
				// a loadstate that needs to create a filepool index),
				// we also send events during the execution (so that
				// we can show progress on the OSD). In that case
				// ignore extra input events.
			}
			return EventDistributor::MSX;
		},
		[](const KeyUpEvent&) { return EventDistributor::MSX; },
		[](const EventBase&) { UNREACHABLE; return EventDistributor::MSX; }
	}, event);
}

bool CommandConsole::handleEvent(const KeyEvent& keyEvent)
{
	auto keyCode = keyEvent.getKeyCode();
	int key = keyCode &  Keys::K_MASK;
	int mod = keyCode & ~Keys::K_MASK;

	switch (mod) {
	case Keys::KM_CTRL:
		switch (key) {
		case Keys::K_H:
			backspace();
			return true;
		case Keys::K_A:
			cursorPosition = unsigned(prompt.size());
			return true;
		case Keys::K_E:
			cursorPosition = unsigned(lines[0].numChars());
			return true;
		case Keys::K_C:
			clearCommand();
			return true;
		case Keys::K_L:
			clearHistory();
			return true;
#ifndef __APPLE__
		case Keys::K_V:
			paste();
			return true;
#endif
		}
		break;
	case Keys::KM_SHIFT:
		switch (key) {
		case Keys::K_PAGEUP:
			scroll(max<int>(getRows() - 1, 1));
			return true;
		case Keys::K_PAGEDOWN:
			scroll(-max<int>(getRows() - 1, 1));
			return true;
		}
		break;
	case Keys::KM_META:
#ifdef __APPLE__
		switch (key) {
		case Keys::K_V:
			paste();
			return true;
		case Keys::K_K:
			clearHistory();
			return true;
		case Keys::K_LEFT:
			cursorPosition = unsigned(prompt.size());
			return true;
		case Keys::K_RIGHT:
			cursorPosition = unsigned(lines[0].numChars());
			return true;
		}
#endif
		break;
	case Keys::KM_ALT:
		switch (key) {
		case Keys::K_BACKSPACE:
			deleteToStartOfWord();
			return true;
#ifdef __APPLE__
		case Keys::K_DELETE:
			deleteToEndOfWord();
			return true;
#else
		case Keys::K_D:
			deleteToEndOfWord();
			return true;
#endif
		case Keys::K_LEFT:
			gotoStartOfWord();
			return true;
		case Keys::K_RIGHT:
			gotoEndOfWord();
			return true;
		}
		break;
	case 0: // no modifier
		switch (key) {
		case Keys::K_PAGEUP:
			scroll(1);
			return true;
		case Keys::K_PAGEDOWN:
			scroll(-1);
			return true;
		case Keys::K_UP:
			prevCommand();
			return true;
		case Keys::K_DOWN:
			nextCommand();
			return true;
		case Keys::K_BACKSPACE:
			backspace();
			return true;
		case Keys::K_DELETE:
			delete_key();
			return true;
		case Keys::K_TAB:
			tabCompletion();
			return true;
		case Keys::K_RETURN:
		case Keys::K_KP_ENTER:
			commandExecute();
			cursorPosition = unsigned(prompt.size());
			return true;
		case Keys::K_LEFT:
			if (cursorPosition > prompt.size()) {
				--cursorPosition;
			}
			return true;
		case Keys::K_RIGHT:
			if (cursorPosition < lines[0].numChars()) {
				++cursorPosition;
			}
			return true;
		case Keys::K_HOME:
#ifdef __APPLE__
			scroll(lines.size());
#else
			cursorPosition = unsigned(prompt.size());
#endif
			return true;
		case Keys::K_END:
#ifdef __APPLE__
			scroll(-lines.size());
#else
			cursorPosition = unsigned(lines[0].numChars());
#endif
			return true;
		}
		break;
	}

	auto unicode = keyEvent.getUnicode();
	if (!unicode || (mod & Keys::KM_META)) {
		// Disallow META modifer for 'normal' key presses because on
		// MacOSX Cmd+L is used as a hotkey to toggle the console.
		// Hopefully there are no systems that require META to type
		// normal keys. However there _are_ systems that require the
		// following modifiers, some examples:
		//  MODE:     to type '1-9' on a N900
		//  ALT:      to type | [ ] on a azerty keyboard layout
		//  CTRL+ALT: to type '#' on a spanish keyboard layout (windows)
		//
		// Event was not used by the console, allow the other
		// subsystems to process it. E.g. F10, or Cmd+L to close the
		// console.
		return false;
	}

	// Apparently on macOS keyboard events for keys like F1 have a non-zero
	// unicode field. This confuses the console code (it thinks it's a
	// printable character) and it prevents those keys from triggering
	// hotkey bindings. See this bug report:
	//   https://github.com/openMSX/openMSX/issues/1095
	// As a workaround we ignore chars in the 'Private Use Area' (PUA).
	//   https://en.wikipedia.org/wiki/Private_Use_Areas
	if (utf8::is_pua(unicode)) {
		return false;
	}

	if (unicode >= 0x20) {
		normalKey(unicode);
	} else {
		// Skip CTRL-<X> combinations, but still return true.
	}
	return true;
}

void CommandConsole::output(string_view text)
{
	print(text);
}

unsigned CommandConsole::getOutputColumns() const
{
	return getColumns();
}

void CommandConsole::print(string_view text, unsigned rgb)
{
	while (true) {
		auto pos = text.find('\n');
		newLineConsole(ConsoleLine(string(text.substr(0, pos)), rgb));
		if (pos == string_view::npos) return;
		text = text.substr(pos + 1); // skip newline
		if (text.empty()) return;
	}
}

void CommandConsole::newLineConsole(string line)
{
	newLineConsole(ConsoleLine(std::move(line)));
}

void CommandConsole::newLineConsole(ConsoleLine line)
{
	if (lines.isFull()) {
		lines.removeBack();
	}
	ConsoleLine tmp = std::move(lines[0]);
	lines[0] = std::move(line);
	lines.addFront(std::move(tmp));
}

void CommandConsole::putCommandHistory(const string& command)
{
	if (command.empty()) return;
	if (removeDoublesSetting.getBoolean() && !history.empty() &&
	    (history.back() == command)) {
		return;
	}
	if (history.full()) history.pop_front();
	history.push_back(command);
}

void CommandConsole::commandExecute()
{
	resetScrollBack();
	string cmd0 = lines[0].str().substr(prompt.size());
	putCommandHistory(cmd0);
	saveHistory(); // save at this point already, so that we don't lose history in case of a crash

	strAppend(commandBuffer, cmd0, '\n');
	newLineConsole(lines[0]);
	if (commandController.isComplete(commandBuffer)) {
		// Normally the busy prompt is NOT shown (not even very briefly
		// because the screen is not redrawn), though for some commands
		// that potentially take a long time to execute, we explicitly
		// send events, see also comment in signalEvent().
		prompt = PROMPT_BUSY;
		putPrompt();

		try {
			ScopedAssign sa(executingCommand, true);
			auto resultObj = commandController.executeCommand(
				commandBuffer);
			auto result = resultObj.getString();
			if (!result.empty()) {
				print(result);
			}
		} catch (CommandException& e) {
			print(e.getMessage(), 0xff0000);
		}
		commandBuffer.clear();
		prompt = PROMPT_NEW;
	} else {
		prompt = PROMPT_CONT;
	}
	putPrompt();
}

ConsoleLine CommandConsole::highLight(string_view line)
{
	ConsoleLine result;
	result.addChunk(prompt, 0xffffff);

	TclParser parser = commandController.getInterpreter().parse(line);
	string colors = parser.getColors();
	assert(colors.size() == line.size());

	unsigned pos = 0;
	while (pos != colors.size()) {
		char col = colors[pos];
		unsigned pos2 = pos++;
		while ((pos != colors.size()) && (colors[pos] == col)) {
			++pos;
		}
		// TODO make these color configurable?
		unsigned rgb = [&] {
			switch (col) {
			case 'E': return 0xff0000; // error
			case 'c': return 0x5c5cff; // comment
			case 'v': return 0x00ffff; // variable
			case 'l': return 0xff00ff; // literal
			case 'p': return 0xcdcd00; // proc
			case 'o': return 0x00cdcd; // operator
			default:  return 0xffffff; // other
			}
		}();
		result.addChunk(line.substr(pos2, pos - pos2), rgb);
	}
	return result;
}

void CommandConsole::putPrompt()
{
	commandScrollBack = unsigned(history.size());
	currentLine.clear();
	lines[0] = highLight(currentLine);
	cursorPosition = unsigned(prompt.size());
}

void CommandConsole::tabCompletion()
{
	resetScrollBack();
	auto pl = unsigned(prompt.size());
	string front(utf8::unchecked::substr(lines[0].str(), pl, cursorPosition - pl));
	string back (utf8::unchecked::substr(lines[0].str(), cursorPosition));
	string newFront = commandController.tabCompletion(front);
	cursorPosition = pl + unsigned(utf8::unchecked::size(newFront));
	currentLine = newFront + back;
	lines[0] = highLight(currentLine);
}

void CommandConsole::scroll(int delta)
{
	consoleScrollBack = max(min(consoleScrollBack + delta, int(lines.size()) - int(rows)), 0);
}

// Returns the position of the start of the word relative to the current cursor position.
// If the cursor is already located at the start of a word then return the start of the previous word.
// If there is no previous word, then return the start of the line (position of the prompt).
//
// One complication is that the line is utf8 encoded and the positions are
// given in character-counts instead of byte-counts. To solve this this
// function returns a tuple:
//  * an iterator to the start of the word (likely the new cursor position).
//  * an iterator to the current cursor position.
//  * the distance (in characters, not bytes) between these two iterators.
// The first item in this tuple is the actual result. The other two are only
// returned for efficiency (this function calculates them anyway, and it's
// likely the caller will need them as well).
static std::tuple<std::string::const_iterator, std::string::const_iterator, unsigned>
	getStartOfWord(const std::string& line, unsigned cursorPos, size_t promptSize)
{
	auto begin  = std::begin(line);
	auto prompt = begin + promptSize; // assumes prompt only contains single-byte utf8 chars
	auto cursor = begin; utf8::unchecked::advance(cursor, cursorPos);
	auto pos    = cursor;

	// search (backwards) for first non-white-space
	unsigned distance = 0;
	while (pos > prompt) {
		auto pos2 = pos;
		utf8::unchecked::prior(pos2);
		if (*pos2 != one_of(' ', '\t')) break;
		pos = pos2;
		++distance;
	}
	// search (backwards) for first white-space
	while (pos > prompt) {
		auto pos2 = pos;
		utf8::unchecked::prior(pos2);
		if (*pos2 == one_of(' ', '\t')) break;
		pos = pos2;
		++distance;
	}
	return {pos, cursor, distance};
}

// Similar to getStartOfWord() but locates the end of the word instead.
// The end of the word is the 2nd (instead of the 1st) element in the tuple.
// This is done so that both getStartOfWord() and getEndOfWord() have the invariant:
//  result = [begin, end, distance]
//   -> begin <= end
//   -> end - begin == distance
static std::tuple<std::string::const_iterator, std::string::const_iterator, unsigned>
	getEndOfWord(const std::string& line, unsigned cursorPos)
{
	auto begin  = std::begin(line);
	auto end    = std::end  (line);
	auto cursor = begin; utf8::unchecked::advance(cursor, cursorPos);
	auto pos    = cursor;

	// search (forwards) for first non-white-space
	unsigned distance = 0;
	while ((pos < end) && (*pos == one_of(' ', '\t'))) {
		++pos; // single-byte utf8 character
		++distance;
	}
	// search (forwards) for first white-space
	while ((pos < end) && (*pos != one_of(' ', '\t'))) {
		utf8::unchecked::next(pos);
		++distance;
	}
	return {cursor, pos, distance};
}

void CommandConsole::gotoStartOfWord()
{
	auto [begin, end, distance] = getStartOfWord(lines[0].str(), cursorPosition, prompt.size());
	cursorPosition -= distance;
}

void CommandConsole::deleteToStartOfWord()
{
	resetScrollBack();
	currentLine = lines[0].str();
	auto [begin, end, distance] = getStartOfWord(currentLine, cursorPosition, prompt.size());
	currentLine.erase(begin, end);
	currentLine.erase(0, prompt.size());
	lines[0] = highLight(currentLine);
	cursorPosition -= distance;
}

void CommandConsole::gotoEndOfWord()
{
	auto [begin, end, distance] = getEndOfWord(lines[0].str(), cursorPosition);
	cursorPosition += distance;
}

void CommandConsole::deleteToEndOfWord()
{
	resetScrollBack();
	currentLine = lines[0].str();
	auto [begin, end, distance] = getEndOfWord(currentLine, cursorPosition);
	currentLine.erase(begin, end);
	currentLine.erase(0, prompt.size());
	lines[0] = highLight(currentLine);
}

void CommandConsole::prevCommand()
{
	resetScrollBack();
	if (history.empty()) {
		return; // no elements
	}
	bool match = false;
	unsigned tmp = commandScrollBack;
	while ((tmp != 0) && !match) {
		--tmp;
		match = StringOp::startsWith(history[tmp], currentLine);
	}
	if (match) {
		commandScrollBack = tmp;
		lines[0] = highLight(history[commandScrollBack]);
		cursorPosition = unsigned(lines[0].numChars());
	}
}

void CommandConsole::nextCommand()
{
	resetScrollBack();
	if (commandScrollBack == history.size()) {
		return; // don't loop !
	}
	bool match = false;
	auto tmp = commandScrollBack;
	while ((++tmp != history.size()) && !match) {
		match = StringOp::startsWith(history[tmp], currentLine);
	}
	if (match) {
		--tmp; // one time to many
		commandScrollBack = tmp;
		lines[0] = highLight(history[commandScrollBack]);
	} else {
		commandScrollBack = unsigned(history.size());
		lines[0] = highLight(currentLine);
	}
	cursorPosition = unsigned(lines[0].numChars());
}

void CommandConsole::clearCommand()
{
	resetScrollBack();
	commandBuffer.clear();
	prompt = PROMPT_NEW;
	currentLine.clear();
	lines[0] = highLight(currentLine);
	cursorPosition = unsigned(prompt.size());
}

void CommandConsole::clearHistory()
{
	resetScrollBack();
	while (lines.size() > 1) {
		lines.removeBack();
	}
}

void CommandConsole::backspace()
{
	resetScrollBack();
	if (cursorPosition > prompt.size()) {
		currentLine = lines[0].str();
		auto b = begin(currentLine);
		utf8::unchecked::advance(b, cursorPosition - 1);
		auto e = b;
		utf8::unchecked::advance(e, 1);
		currentLine.erase(b, e);
		currentLine.erase(0, prompt.size());
		lines[0] = highLight(currentLine);
		--cursorPosition;
	}
}

void CommandConsole::delete_key()
{
	resetScrollBack();
	if (lines[0].numChars() > cursorPosition) {
		currentLine = lines[0].str();
		auto b = begin(currentLine);
		utf8::unchecked::advance(b, cursorPosition);
		auto e = b;
		utf8::unchecked::advance(e, 1);
		currentLine.erase(b, e);
		currentLine.erase(0, prompt.size());
		lines[0] = highLight(currentLine);
	}
}

void CommandConsole::normalKey(uint32_t chr)
{
	assert(chr);
	resetScrollBack();
	currentLine = lines[0].str();
	auto pos = begin(currentLine);
	utf8::unchecked::advance(pos, cursorPosition);
	utf8::unchecked::append(chr, inserter(currentLine, pos));
	currentLine.erase(0, prompt.size());
	lines[0] = highLight(currentLine);
	++cursorPosition;
}

void CommandConsole::resetScrollBack()
{
	consoleScrollBack = 0;
}

static std::vector<std::string_view> splitLines(std::string_view str)
{
	// This differs from StringOp::split() in two ways:
	// - If the input is an empty string, then the resulting vector
	//   contains 1 element which is the empty string (StringOp::split()
	//   would return an empty vector).
	// - If the input ends on a newline character, then the final item in
	//   the result vector is an empty string (StringOp::split() would not
	//   include that last empty string).
	// TODO can we come up with a good name for this function and move it
	//      to StringOp?
	std::vector<std::string_view> result;
	while (true) {
		auto pos = str.find_first_of('\n');
		if (pos == std::string_view::npos) break;
		result.push_back(str.substr(0, pos));
		str = str.substr(pos + 1);
	}
	result.push_back(str);
	return result;
}

void CommandConsole::paste()
{
	auto text = display.getVideoSystem().getClipboardText();
	if (text.empty()) return;
	auto pastedLines = splitLines(text);
	assert(!pastedLines.empty());

	// helper function 'append()' to append to the existing text
	std::string_view prefix = lines[0].str();
	prefix.remove_prefix(prompt.size());
	auto append = [&](std::string_view suffix) {
		if (prefix.empty()) {
			lines[0] = highLight(suffix);
		} else {
			lines[0] = highLight(tmpStrCat(prefix, suffix));
			prefix = "";
		}
	};
	// execute all but the last line
	for (const auto& line : view::drop_back(pastedLines, 1)) {
		append(line);
		commandExecute();
	}
	// only enter (not execute) the last line
	append(pastedLines.back());
	cursorPosition = unsigned(lines[0].numChars());
}

} // namespace openmsx
