// $Id$

#include "CommandConsole.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "Keys.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "SettingsManager.hh"
#include "CliCommOutput.hh"
#include "MSXConfig.hh"
#include "Config.hh"


namespace openmsx {

// class CommandConsole

const string PROMPT("> ");

CommandConsole::CommandConsole()
	: consoleSetting("console", "turns console display on/off", false)
{
	SDL_EnableUNICODE(1);
	consoleSetting.addListener(this);
	EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->registerEventListener(SDL_KEYUP,   this);
	putPrompt();
	Config* config = MSXConfig::instance()->getConfigById("Console");
	maxHistory = config->getParameterAsInt("historysize", 100);
	removeDoubles = config->getParameterAsBool("removedoubles", true);
	loadHistory();
}

CommandConsole::~CommandConsole()
{
	saveHistory();
	EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->unregisterEventListener(SDL_KEYUP,   this);
	consoleSetting.removeListener(this);
}

CommandConsole *CommandConsole::instance()
{
	static CommandConsole oneInstance;
	return &oneInstance;
}

void CommandConsole::update(const SettingLeafNode *setting)
{
	assert(setting == &consoleSetting);
	updateConsole();
	if (consoleSetting.getValue()) {
		SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
		                    SDL_DEFAULT_REPEAT_INTERVAL);
	} else {
		SDL_EnableKeyRepeat(0, 0);
	}
}

void CommandConsole::saveHistory()
{
	const string filename("history.txt");
	UserFileContext context("console");
	try {
		ofstream outputfile(FileOperations::expandTilde(
		        context.resolveSave(filename)).c_str());
		if (!outputfile) {
			throw FileException(
				"Error while saving the consolehistory: " + filename);
		}
		list<string>::iterator it;
		for (it = history.begin(); it != history.end(); it++) {
			outputfile << it->substr(PROMPT.length()) << endl;
		}
	} catch (FileException &e) {
		CliCommOutput::instance().printWarning(e.getMessage());
	}
}

void CommandConsole::loadHistory()
{
	const string filename("history.txt");
	UserFileContext context("console");
	try {
		string line;
		ifstream inputfile(FileOperations::expandTilde(
		        context.resolveSave(filename)).c_str());
		if (!inputfile) {
			throw FileException(
				"Error while loading the consolehistory: " + filename);
		}
		while (inputfile) {
			getline(inputfile, line);
			if (!line.empty()) {
				line.insert(0, PROMPT);
				putCommandHistory(line);
			}
		}
	} catch (FileException &e) {
		PRT_DEBUG(e.getMessage());
	}
}

void CommandConsole::setCursorPosition(unsigned xPosition, unsigned yPosition)
{
	if (xPosition > lines[0].length()) {
		cursorLocationX = lines[0].length();
	} else if (xPosition < PROMPT.length()) {
		cursorLocationX = PROMPT.length();
	} else {
		cursorLocationX = xPosition;
	}
	cursorLocationY = yPosition;
}

void CommandConsole::getCursorPosition(unsigned& xPosition, unsigned& yPosition) const
{
	xPosition = cursorLocationX;
	yPosition = cursorLocationY;
} 

unsigned CommandConsole::getScrollBack() const
{
	return consoleScrollBack;
}

const string& CommandConsole::getLine(unsigned line) const
{
	static string EMPTY;
	return line < lines.size() ? lines[line] : EMPTY;
}

bool CommandConsole::isVisible() const
{
	return consoleSetting.getValue();
}

void CommandConsole::setConsoleDimensions(unsigned columns, unsigned rows)
{
	consoleRows = rows;
	if (consoleColumns == columns) {
		return;
	}
	consoleColumns = columns;
	
	CircularBuffer<string, LINESHISTORY> linesbackup;
	CircularBuffer<bool, LINESHISTORY> flowbackup;
	
	while (lines.size() > 0) {
		linesbackup.addBack(lines[0]);
		flowbackup.addBack(lineOverflows[0]);
		lines.removeFront();
		lineOverflows.removeFront();
	}
	while (linesbackup.size() > 0) {
		combineLines(linesbackup, flowbackup, true);
		splitLines();
	}
	cursorLocationX = lines[0].length();
	cursorLocationY = 0;
}

bool CommandConsole::signalEvent(SDL_Event &event) throw()
{
	if (!isVisible()) {
		return true;
	}
	if (event.type == SDL_KEYUP) {
		return false;	// don't pass event to MSX-Keyboard
	}

	Keys::KeyCode key = (Keys::KeyCode)event.key.keysym.sym;
	SDLMod modifier = event.key.keysym.mod;
	switch (key) {
		case Keys::K_PAGEUP:
			scrollUp();
			break;
		case Keys::K_PAGEDOWN:
			scrollDown();
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
			delete_key();	// sorry delete is reserved
			break;
		case Keys::K_TAB:
			tabCompletion();
			break;
		case Keys::K_RETURN:
			commandExecute();
			cursorLocationX = PROMPT.length();
			break;
		case Keys::K_LEFT:
			combineLines(lines, lineOverflows);
			if (cursorPosition > PROMPT.length()) {
				cursorPosition--;
			}
			splitLines();
			break;
		case Keys::K_RIGHT:
			combineLines(lines, lineOverflows);
			if (cursorPosition < editLine.length()) {
				cursorPosition++;
			}
			splitLines();
			break;
		case Keys::K_HOME:
			combineLines(lines, lineOverflows);
			cursorPosition = PROMPT.length();
			splitLines();
			break;
		case Keys::K_END:
			combineLines(lines, lineOverflows);
			cursorPosition = editLine.length();
			splitLines();
			break;
		case Keys::K_A:
			if (modifier & (KMOD_LCTRL | KMOD_RCTRL)) {
				combineLines(lines, lineOverflows);
				cursorPosition = PROMPT.length();
				splitLines();
			} else {
				normalKey((char)event.key.keysym.unicode);	
			}
			break;	
		case Keys::K_C:
			if (modifier & (KMOD_LCTRL | KMOD_RCTRL)) {
				clearCommand();
			} else {
				normalKey((char)event.key.keysym.unicode);	
			}
			break;
		case Keys::K_E:
			if (modifier & (KMOD_LCTRL | KMOD_RCTRL)) {
				combineLines(lines, lineOverflows);
				cursorPosition = editLine.length();
				splitLines();
			} else {
				normalKey((char)event.key.keysym.unicode);
			}
			break;
		default:
			normalKey((char)event.key.keysym.unicode);
	}
	updateConsole();
	return false;	// don't pass event to MSX-Keyboard
}

void CommandConsole::combineLines(CircularBuffer<string, LINESHISTORY> &buffer,
                           CircularBuffer<bool, LINESHISTORY> &overflows,
                           bool fromTop)
{
	int startline;
	int totallines = 0;
	editLine = "";
	
	if (fromTop) {
		startline = buffer.size() - 1;
		while (((startline - totallines) > 0) && 
		       (overflows[startline - totallines])) {
			totallines++;
		}
		for (int i = startline; i >= startline-totallines; --i) {
			editLine += buffer[i];

		}
		for (int i = 0; i < (totallines + 1); ++i) {
			buffer.removeBack();
			overflows.removeBack();
		}
	} else {
		startline = 0;
		while (((startline + totallines + 1) < (int)buffer.size()) && 
		       (overflows[startline + totallines + 1])) {
			totallines++;
		}
		for (int i = totallines; i >= 0; --i) {
			editLine += buffer[i];
		}
		for (int i = 0; i < (totallines + 1); ++i) {
			buffer.removeFront();
			overflows.removeFront();
		}
	}
	
	int temp = totallines - cursorLocationY;
	cursorPosition = (consoleColumns * temp) + cursorLocationX;
}

void CommandConsole::splitLines()
{
	unsigned numberOfLines = 1 + (editLine.length() / consoleColumns);
	for (unsigned i = 1; i <= numberOfLines; ++i) {
		newLineConsole(editLine.substr(consoleColumns * (i - 1),
		               consoleColumns));
		lineOverflows[0] = (i != numberOfLines);
	}
	cursorLocationX = cursorPosition % consoleColumns;
	unsigned temp = cursorPosition / consoleColumns;
	cursorLocationY = numberOfLines - 1 - temp;
}

void CommandConsole::printFast(const string &text)
{
	unsigned end = 0;
	do {
		unsigned start = end;
		end = text.find('\n', start);
		if (end == string::npos) {
			end = text.length();
		}
		if ((end - start) > (consoleColumns - 1)) {
			end = start + consoleColumns;
			newLineConsole(text.substr(start, end - start));
			lineOverflows[0] = true; 
		} else {
			newLineConsole(text.substr(start, end - start));
			lineOverflows[0] = false;
			++end; // skip newline
		}
	} while (end < text.length());
}

void CommandConsole::printFlush()
{
	updateConsole();
}

void CommandConsole::print(const string &text)
{
	printFast(text);
	printFlush();
}

void CommandConsole::newLineConsole(const string &line)
{
	if (lines.isFull()) {
		lines.removeBack();
		lineOverflows.removeBack();	
	}
	lines.addFront(line);
	lineOverflows.addFront(false);
}

void CommandConsole::putCommandHistory(const string &command)
{
	// TODO don't store PROMPT as part of history
	if (command == PROMPT) {
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
	combineLines(lines, lineOverflows);
	putCommandHistory(editLine);
	splitLines();
	try {
		string result = CommandController::instance()->
			executeCommand(editLine.substr(PROMPT.length()));
		if (!result.empty()) {
			print(result);
		}
	} catch (CommandException &e) {
		print(e.getMessage());
	}
	putPrompt();
}

void CommandConsole::putPrompt()
{
	newLineConsole(PROMPT);
	consoleScrollBack = 0;
	commandScrollBack = history.end();
	currentLine=PROMPT;
	cursorLocationX = PROMPT.length();
	cursorLocationY = 0;
}

void CommandConsole::tabCompletion()
{
	unsigned pl = PROMPT.length();

	resetScrollBack();
	combineLines(lines, lineOverflows);
	string front(editLine.substr(pl, cursorPosition - pl));
	string back(editLine.substr(cursorPosition));
	CommandController::instance()->tabCompletion(front);
	cursorPosition = pl + front.length();
	editLine = PROMPT + front + back;
	currentLine = editLine;
	splitLines();
}

void CommandConsole::scrollUp()
{
	if (consoleScrollBack < lines.size()) {
		consoleScrollBack++;
	}
}

void CommandConsole::scrollDown()
{
	if (consoleScrollBack > 0) {
		consoleScrollBack--;
	}
}

void CommandConsole::prevCommand()
{
	list<string>::iterator tempScrollBack = commandScrollBack;
	bool match = false;
	resetScrollBack();
	if (history.empty()) {
		return; // no elements
	}
	combineLines(lines, lineOverflows);
	while ((tempScrollBack != history.begin()) && !match) {
		tempScrollBack--;
		match = ((tempScrollBack->length() >= currentLine.length()) &&
		         (tempScrollBack->substr(0, currentLine.length()) == currentLine));
	}
	if (match) {
		commandScrollBack = tempScrollBack;
		editLine = *commandScrollBack;
		cursorPosition = editLine.length();
	}
	splitLines();
}

void CommandConsole::nextCommand()
{
	if (commandScrollBack == history.end()) {
		return; // don't loop !
	}
	list<string>::iterator tempScrollBack = commandScrollBack;
	bool match = false;
	resetScrollBack();
	combineLines(lines, lineOverflows);
	while ((++tempScrollBack != history.end()) && !match) {
		match = ((tempScrollBack->length() >= currentLine.length()) &&
		         (tempScrollBack->substr(0, currentLine.length()) == currentLine));
	}
	if (match) {
		--tempScrollBack; // one time to many
		commandScrollBack = tempScrollBack;
		editLine = *commandScrollBack;
		cursorPosition = editLine.length();
	} else {
		commandScrollBack = history.end();
		editLine = currentLine;
	}
	cursorPosition = editLine.length();
	splitLines();
}

void CommandConsole::clearCommand()
{
	resetScrollBack();
	combineLines(lines, lineOverflows);
	editLine = currentLine = PROMPT;
	cursorPosition = PROMPT.length();
	splitLines();
}

void CommandConsole::backspace()
{
	resetScrollBack();
	combineLines(lines, lineOverflows);
	if (cursorPosition > PROMPT.length()) {
		string temp;
		temp = editLine.substr(cursorPosition);
		editLine.erase(cursorPosition - 1);
		editLine += temp;
		cursorPosition--;
		currentLine = editLine;
	}
	splitLines();
}

void CommandConsole::delete_key()
{
	resetScrollBack();
	combineLines(lines, lineOverflows);
	if (editLine.length() > cursorPosition) {
		string temp;
		temp = editLine.substr(cursorPosition + 1);
		editLine.erase(cursorPosition);
		editLine += temp;
		currentLine = editLine;
	}
	splitLines();
}

void CommandConsole::normalKey(char chr)
{
	if (!chr) {
		return;
	}
	resetScrollBack();
	combineLines(lines, lineOverflows);
	string temp;
	temp += chr;
	editLine.insert(cursorPosition, temp);
	cursorPosition++;
	currentLine = editLine;
	splitLines();
}

void CommandConsole::resetScrollBack()
{
	consoleScrollBack = 0;
}

void CommandConsole::registerDebugger()
{
	SettingLeafNode* debugSetting = dynamic_cast<SettingLeafNode*>(
		SettingsManager::instance()->getByName("debugger"));
	debugSetting->addListener(this);
}

const string& CommandConsole::getId() const
{
	static const string ID("console");
	return ID;
}

} // namespace openmsx
