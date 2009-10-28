// $Id$

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
#include "SettingsConfig.hh"
#include "XMLElement.hh"
#include "InputEvents.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "Version.hh"
#include "checked_cast.hh"
#include "utf8_unchecked.hh"
#include "StringOp.hh"
#include <algorithm>
#include <fstream>
#include <cassert>

using std::min;
using std::max;
using std::string;

namespace openmsx {

// class CommandConsole

const char* const PROMPT1 = "> ";
const char* const PROMPT2 = "| ";

CommandConsole::CommandConsole(
		GlobalCommandController& commandController_,
		EventDistributor& eventDistributor_,
		Display& display_)
	: commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, display(display_)
	, consoleSetting(new BooleanSetting(commandController, "console",
		"turns console display on/off", false, Setting::DONT_SAVE))
	, historySizeSetting(new IntegerSetting(commandController, "console_history_size",
		"amount of commands kept in console history", 100, 0, 10000))
	, removeDoublesSetting(new BooleanSetting(commandController,
		"console_remove_doubles", "don't add the command to history if "
		"it's the same as the previous one", true))
{
	resetScrollBack();
	prompt = PROMPT1;
	newLineConsole(prompt);
	putPrompt();
	loadHistory();
	Completer::setOutput(this);

	print(Version::FULL_VERSION);
	print(string(Version::FULL_VERSION.size(), '-'));
	print("\n"
	      "General information about openMSX is available at "
	      "http://www.openmsx.org.\n"
	      "\n"
	      "Type 'help' to see a list of available commands "
	      "(use <PgUp>/<PgDn> to scroll).\n"
	      "Or read the Console Command Reference in the manual.\n"
	      "\n");

	commandController.getInterpreter().setOutput(this);
	eventDistributor.registerEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this, EventDistributor::CONSOLE);
	// also listen to KEY_UP events, so that we can consume them
	eventDistributor.registerEventListener(
		OPENMSX_KEY_UP_EVENT, *this, EventDistributor::CONSOLE);
}

CommandConsole::~CommandConsole()
{
	eventDistributor.unregisterEventListener(OPENMSX_KEY_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_KEY_UP_EVENT, *this);
	commandController.getInterpreter().setOutput(NULL);
	Completer::setOutput(NULL);
}

void CommandConsole::saveHistory()
{
	try {
		UserFileContext context("console");
		std::ofstream outputfile;
		FileOperations::openofstream(outputfile,
		        context.resolveCreate("history.txt"));
		if (!outputfile) {
			throw FileException(
				"Error while saving the console history.");
		}
		for (History::const_iterator it = history.begin();
		     it != history.end(); ++it) {
			outputfile << it->substr(prompt.size()) << std::endl;
		}
	} catch (FileException& e) {
		commandController.getCliComm().printWarning(e.getMessage());
	}
}

void CommandConsole::loadHistory()
{
	try {
		UserFileContext context("console");
		std::ifstream inputfile(
		        context.resolveCreate("history.txt").c_str());
		if (!inputfile) {
			throw FileException(
				"Error while loading the console history.");
		}
		string line;
		while (inputfile) {
			getline(inputfile, line);
			if (!line.empty()) {
				putCommandHistory(prompt + line);
			}
		}
	} catch (FileException& e) {
		PRT_DEBUG(e.getMessage());
		(void)&e; // Prevent warning
	}
}

BooleanSetting& CommandConsole::getConsoleSetting()
{
	return *consoleSetting.get();
}

void CommandConsole::getCursorPosition(unsigned& xPosition, unsigned& yPosition) const
{
	xPosition = cursorPosition % getColumns();
	unsigned num = utf8::unchecked::size(lines[0]) / getColumns();
	yPosition = num - (cursorPosition / getColumns());
}

unsigned CommandConsole::getScrollBack() const
{
	return consoleScrollBack;
}

string CommandConsole::getLine(unsigned line) const
{
	unsigned count = 0;
	for (unsigned buf = 0; buf < lines.size(); ++buf) {
		count += (utf8::unchecked::size(lines[buf]) / getColumns()) + 1;
		if (count > line) {
			return utf8::unchecked::substr(
				lines[buf],
				(count - line - 1) * getColumns(),
				getColumns());
		}
	}
	return "";
}

bool CommandConsole::signalEvent(shared_ptr<const Event> event)
{
	const KeyEvent& keyEvent = checked_cast<const KeyEvent&>(*event);
	if (consoleSetting->getValue()) {
		if (event->getType() == OPENMSX_KEY_DOWN_EVENT) {
			handleEvent(keyEvent);
			display.repaintDelayed(40000); // 25fps
			return false; // deny event to other listeners
		} else if (event->getType() == OPENMSX_KEY_UP_EVENT) {
			// consume this event so it won't leak to the MSX
			return false; // deny event to other listeners
		}
	}
	return true;
}

void CommandConsole::handleEvent(const KeyEvent& keyEvent)
{
	Keys::KeyCode keyCode = keyEvent.getKeyCode();
	int key = keyCode &  Keys::K_MASK;
	int mod = keyCode & ~Keys::K_MASK;
	switch (mod) {
	case Keys::KM_CTRL:
		switch (key) {
		case Keys::K_H:
			backspace();
			break;
		case Keys::K_A:
			cursorPosition = unsigned(prompt.size());
			break;
		case Keys::K_E:
			cursorPosition = utf8::unchecked::size(lines[0]);
			break;
		case Keys::K_C:
			clearCommand();
			break;
		}
		break;
	case Keys::KM_SHIFT:
		switch (key) {
		case Keys::K_PAGEUP:
			scroll(max<int>(getRows() - 1, 1));
			break;
		case Keys::K_PAGEDOWN:
			scroll(-max<int>(getRows() - 1, 1));
			break;
		default:
			normalKey(keyEvent.getUnicode());
			break;
		}
		break;
	case 0:
		switch (key) {
		case Keys::K_PAGEUP:
			scroll(1);
			break;
		case Keys::K_PAGEDOWN:
			scroll(-1);
			break;
		case Keys::K_UP:
			prevCommand();
			break;
		case Keys::K_DOWN:
			nextCommand();
			break;
		case Keys::K_BACKSPACE:
			backspace();
			break;
		case Keys::K_DELETE:
			delete_key();
			break;
		case Keys::K_TAB:
			tabCompletion();
			break;
		case Keys::K_RETURN:
		case Keys::K_KP_ENTER:
			commandExecute();
			cursorPosition = unsigned(prompt.size());
			break;
		case Keys::K_LEFT:
			if (cursorPosition > prompt.size()) {
				--cursorPosition;
			}
			break;
		case Keys::K_RIGHT:
			if (cursorPosition < utf8::unchecked::size(lines[0])) {
				++cursorPosition;
			}
			break;
		case Keys::K_HOME:
			cursorPosition = unsigned(prompt.size());
			break;
		case Keys::K_END:
			cursorPosition = utf8::unchecked::size(lines[0]);
			break;
		default:
			normalKey(keyEvent.getUnicode());
			break;
		}
		break;
	}
}

void CommandConsole::setColumns(unsigned columns_)
{
	columns = columns_;
}

unsigned CommandConsole::getColumns() const
{
	return columns;
}

void CommandConsole::setRows(unsigned rows_)
{
	rows = rows_;
}

unsigned CommandConsole::getRows() const
{
	return rows;
}

void CommandConsole::output(const std::string& text)
{
	print(text);
}

unsigned CommandConsole::getOutputColumns() const
{
	return getColumns();
}

void CommandConsole::print(string text)
{
	if (text.empty() || (*text.rbegin() != '\n')) text += '\n';
	while (!text.empty()) {
		string::size_type pos = text.find('\n');
		newLineConsole(text.substr(0, pos));
		text = text.substr(pos + 1); // skip newline
	}
}

void CommandConsole::newLineConsole(const string& line)
{
	if (lines.isFull()) {
		lines.removeBack();
	}
	string tmp = lines[0];
	lines[0] = line;
	lines.addFront(tmp);
}

void CommandConsole::putCommandHistory(const string& command)
{
	// TODO don't store PROMPT as part of history
	if (command == prompt) {
		return;
	}
	if (removeDoublesSetting->getValue() && !history.empty()
		&& (history.back() == command)) {
		return;
	}

	history.push_back(command);

	// if necessary, shrink history to the desired (smaller) size
	while (history.size() > unsigned(historySizeSetting->getValue())) {
		history.pop_front();
	}
}

void CommandConsole::commandExecute()
{
	resetScrollBack();
	putCommandHistory(lines[0]);
	saveHistory(); // save at this point already, so that we don't lose history in case of a crash

	commandBuffer += lines[0].substr(prompt.size()) + '\n';
	newLineConsole(lines[0]);
	if (commandController.isComplete(commandBuffer)) {
		prompt = PROMPT1;
		putPrompt();
		try {
			string result = commandController.executeCommand(
				commandBuffer);
			if (!result.empty()) {
				print(result);
			}
		} catch (CommandException& e) {
			print(e.getMessage());
		}
		commandBuffer.clear();
	} else {
		prompt = PROMPT2;
		putPrompt();
	}
}

void CommandConsole::putPrompt()
{
	commandScrollBack = history.end();
	currentLine = lines[0] = prompt;
	cursorPosition = unsigned(prompt.size());
}

void CommandConsole::tabCompletion()
{
	resetScrollBack();
	string::size_type pl = prompt.size();
	string front = utf8::unchecked::substr(lines[0], pl, cursorPosition - pl);
	string back  = utf8::unchecked::substr(lines[0], cursorPosition);
	commandController.tabCompletion(front);
	cursorPosition = unsigned(pl + utf8::unchecked::size(front));
	currentLine = lines[0] = prompt + front + back;
}

void CommandConsole::scroll(int delta)
{
	consoleScrollBack = min<int>(max(consoleScrollBack + delta, 0),
	                             lines.size());
}

void CommandConsole::prevCommand()
{
	resetScrollBack();
	if (history.empty()) {
		return; // no elements
	}
	bool match = false;
	History::const_iterator tempScrollBack = commandScrollBack;
	while ((tempScrollBack != history.begin()) && !match) {
		tempScrollBack--;
		match = StringOp::startsWith(*tempScrollBack, currentLine);
	}
	if (match) {
		commandScrollBack = tempScrollBack;
		lines[0] = *commandScrollBack;
		cursorPosition = utf8::unchecked::size(lines[0]);
	}
}

void CommandConsole::nextCommand()
{
	resetScrollBack();
	if (commandScrollBack == history.end()) {
		return; // don't loop !
	}
	bool match = false;
	History::const_iterator tempScrollBack = commandScrollBack;
	while ((++tempScrollBack != history.end()) && !match) {
		match = StringOp::startsWith(*tempScrollBack, currentLine);
	}
	if (match) {
		--tempScrollBack; // one time to many
		commandScrollBack = tempScrollBack;
		lines[0] = *commandScrollBack;
	} else {
		commandScrollBack = history.end();
		lines[0] = currentLine;
	}
	cursorPosition = utf8::unchecked::size(lines[0]);
}

void CommandConsole::clearCommand()
{
	resetScrollBack();
	commandBuffer.clear();
	prompt = PROMPT1;
	currentLine = lines[0] = prompt;
	cursorPosition = unsigned(prompt.size());
}

void CommandConsole::backspace()
{
	resetScrollBack();
	if (cursorPosition > prompt.size()) {
		string::iterator begin = lines[0].begin();
		utf8::unchecked::advance(begin, cursorPosition - 1);
		string::iterator end = begin;
		utf8::unchecked::advance(end, 1);
		lines[0].erase(begin, end);
		currentLine = lines[0];
		--cursorPosition;
	}
}

void CommandConsole::delete_key()
{
	resetScrollBack();
	if (utf8::unchecked::size(lines[0]) > cursorPosition) {
		string::iterator begin = lines[0].begin();
		utf8::unchecked::advance(begin, cursorPosition);
		string::iterator end = begin;
		utf8::unchecked::advance(end, 1);
		lines[0].erase(begin, end);
		currentLine = lines[0];
	}
}

void CommandConsole::normalKey(word chr)
{
	if (!chr) return;
	resetScrollBack();
	string::iterator pos = lines[0].begin();
	utf8::unchecked::advance(pos, cursorPosition);
	utf8::unchecked::append(uint32_t(chr), inserter(lines[0], pos));
	currentLine = lines[0];
	++cursorPosition;
}

void CommandConsole::resetScrollBack()
{
	consoleScrollBack = 0;
}

} // namespace openmsx
