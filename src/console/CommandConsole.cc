// $Id$

#include "CommandConsole.hh"
#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "Completer.hh"
#include "Interpreter.hh"
#include "Keys.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "CliComm.hh"
#include "SettingsConfig.hh"
#include "XMLElement.hh"
#include "InputEvents.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "Version.hh"
#include "checked_cast.hh"
#include <algorithm>
#include <fstream>
#include <cassert>

using std::min;
using std::max;
using std::list;
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
	, consoleSetting(commandController.getGlobalSettings().getConsoleSetting())
{
	resetScrollBack();
	prompt = PROMPT1;
	newLineConsole(prompt);
	putPrompt();
	maxHistory = 100;
	removeDoubles = true;
	if (const XMLElement* config = commandController.getSettingsConfig().
		                        getXMLElement().findChild("Console")) {
		maxHistory = config->getChildDataAsInt(
			"historysize", maxHistory);
		removeDoubles = config->getChildDataAsBool(
			"removedoubles", removeDoubles);
	}
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
}

CommandConsole::~CommandConsole()
{
	eventDistributor.unregisterEventListener(OPENMSX_KEY_DOWN_EVENT, *this);
	commandController.getInterpreter().setOutput(NULL);
	Completer::setOutput(NULL);
	saveHistory();
}

void CommandConsole::saveHistory()
{
	try {
		UserFileContext context(commandController, "console");
		std::ofstream outputfile(FileOperations::expandTilde(
		        context.resolveCreate("history.txt")).c_str());
		if (!outputfile) {
			throw FileException(
				"Error while saving the console history.");
		}
		for (list<string>::iterator it = history.begin();
		     it != history.end(); ++it) {
			outputfile << it->substr(prompt.length()) << std::endl;
		}
	} catch (FileException& e) {
		commandController.getCliComm().printWarning(e.getMessage());
	}
}

void CommandConsole::loadHistory()
{
	try {
		UserFileContext context(commandController, "console");
		std::ifstream inputfile(FileOperations::expandTilde(
		        context.resolveCreate("history.txt")).c_str());
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
	}
}

void CommandConsole::getCursorPosition(unsigned& xPosition, unsigned& yPosition) const
{
	xPosition = cursorPosition % getColumns();
	unsigned num = lines[0].size() / getColumns();
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
		count += (lines[buf].size() / getColumns()) + 1;
		if (count > line) {
			return lines[buf].substr((count - line - 1) * getColumns(),
			                         getColumns());
		}
	}
	return "";
}

bool CommandConsole::signalEvent(shared_ptr<const Event> event)
{
	const KeyEvent& keyEvent = checked_cast<const KeyEvent&>(*event);
	if ((event->getType() == OPENMSX_KEY_DOWN_EVENT) &&
	    consoleSetting.getValue()) {
		handleEvent(keyEvent);
		display.repaintDelayed(40000); // 25fps
		return false; // deny event to other listeners
	}
	return true;
}

void CommandConsole::handleEvent(const KeyEvent& keyEvent)
{
	Keys::KeyCode keyCode = keyEvent.getKeyCode();
	switch (keyCode) {
		case (Keys::K_PAGEUP | Keys::KM_SHIFT):
			scroll(max<int>(getRows() - 1, 1));
			break;
		case Keys::K_PAGEUP:
			scroll(1);
			break;
		case (Keys::K_PAGEDOWN | Keys::KM_SHIFT):
			scroll(-max<int>(getRows() - 1, 1));
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
		case Keys::K_H | Keys::KM_CTRL:
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
			cursorPosition = prompt.length();
			break;
		case Keys::K_LEFT:
			if (cursorPosition > prompt.length()) {
				--cursorPosition;
			}
			break;
		case Keys::K_RIGHT:
			if (cursorPosition < lines[0].length()) {
				++cursorPosition;
			}
			break;
		case Keys::K_HOME:
		case Keys::K_A | Keys::KM_CTRL:
			cursorPosition = prompt.length();
			break;
		case Keys::K_END:
		case Keys::K_E | Keys::KM_CTRL:
			cursorPosition = lines[0].length();
			break;
		case Keys::K_C | Keys::KM_CTRL:
			clearCommand();
			break;
		default:
			// Treat as normal key if no modifiers except shift.
			if (!(keyCode & ~(Keys::K_MASK | Keys::KM_SHIFT))) {
				normalKey(keyEvent.getUnicode());
			}
	}
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
	if (removeDoubles && !history.empty() && (history.back() == command)) {
		return;
	}

	history.push_back(command);
	if (history.size() > maxHistory) {
		history.pop_front();
	}
}

void CommandConsole::commandExecute()
{
	resetScrollBack();
	putCommandHistory(lines[0]);

	commandBuffer += lines[0].substr(prompt.length()) + '\n';
	newLineConsole(lines[0]);
	if (commandController.isComplete(commandBuffer)) {
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
		prompt = PROMPT1;
	} else {
		prompt = PROMPT2;
	}
	putPrompt();
}

void CommandConsole::putPrompt()
{
	commandScrollBack = history.end();
	currentLine = lines[0] = prompt;
	cursorPosition = prompt.length();
}

void CommandConsole::tabCompletion()
{
	resetScrollBack();
	string::size_type pl = prompt.length();
	string front(lines[0].substr(pl, cursorPosition - pl));
	string back(lines[0].substr(cursorPosition));
	commandController.tabCompletion(front);
	cursorPosition = pl + front.length();
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
	list<string>::iterator tempScrollBack = commandScrollBack;
	while ((tempScrollBack != history.begin()) && !match) {
		tempScrollBack--;
		match = ((tempScrollBack->length() >= currentLine.length()) &&
		         (tempScrollBack->substr(0, currentLine.length()) == currentLine));
	}
	if (match) {
		commandScrollBack = tempScrollBack;
		lines[0] = *commandScrollBack;
		cursorPosition = lines[0].length();
	}
}

void CommandConsole::nextCommand()
{
	resetScrollBack();
	if (commandScrollBack == history.end()) {
		return; // don't loop !
	}
	bool match = false;
	list<string>::iterator tempScrollBack = commandScrollBack;
	while ((++tempScrollBack != history.end()) && !match) {
		match = ((tempScrollBack->length() >= currentLine.length()) &&
		         (tempScrollBack->substr(0, currentLine.length()) == currentLine));
	}
	if (match) {
		--tempScrollBack; // one time to many
		commandScrollBack = tempScrollBack;
		lines[0] = *commandScrollBack;
	} else {
		commandScrollBack = history.end();
		lines[0] = currentLine;
	}
	cursorPosition = lines[0].length();
}

void CommandConsole::clearCommand()
{
	resetScrollBack();
	currentLine = lines[0] = prompt;
	cursorPosition = prompt.length();
}

void CommandConsole::backspace()
{
	resetScrollBack();
	if (cursorPosition > prompt.length()) {
		lines[0].erase(lines[0].begin() + cursorPosition - 1);
		currentLine = lines[0];
		--cursorPosition;
	}
}

void CommandConsole::delete_key()
{
	resetScrollBack();
	if (lines[0].length() > cursorPosition) {
		lines[0].erase(lines[0].begin() + cursorPosition);
		currentLine = lines[0];
	}
}

void CommandConsole::normalKey(word chr)
{
	if (!chr) return;
	if (chr >= 0x100) return; // TODO: Support Unicode (how?).
	resetScrollBack();
	lines[0].insert(lines[0].begin() + cursorPosition, char(chr));
	currentLine = lines[0];
	++cursorPosition;
}

void CommandConsole::resetScrollBack()
{
	consoleScrollBack = 0;
}

} // namespace openmsx
