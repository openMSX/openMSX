// $Id$

#include "Console.hh"
#include "OSDConsoleRenderer.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "Keys.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileOperations.hh"

// class ConsoleSetting

Console::ConsoleSetting::ConsoleSetting(Console *console_)
	: BooleanSetting("console", "turns console display on/off", false),
	  console(console_)
{
}

bool Console::ConsoleSetting::checkUpdate(bool newValue)
{
	console->updateConsole();
	if (newValue) {
		SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
		                    SDL_DEFAULT_REPEAT_INTERVAL);
	} else {
		SDL_EnableKeyRepeat(0,0);
	}
	return true;
}


// class Console

const std::string PROMPT("> ");

Console::Console()
	: consoleSetting(this)
{
	SDL_EnableUNICODE(1);
	EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->registerEventListener(SDL_KEYUP,   this);
	putPrompt();
	Config *config = MSXConfig::instance()->getConfigById("Console");
	if (config->hasParameter("historysize")) {
		maxHistory = config->getParameterAsInt("historysize");
	} else {
		maxHistory = 100;
	}
	if (config->hasParameter("removedoubles")){
		removeDoubles = config->getParameterAsBool("removedoubles");
	} else {
		removeDoubles = true;
	}
	loadHistory();
}

Console::~Console()
{
	saveHistory();
	EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->unregisterEventListener(SDL_KEYUP,   this);
}

Console *Console::instance()
{
	static Console oneInstance;
	return &oneInstance;
}

void Console::registerConsole(ConsoleRenderer *console)
{
	renderers.push_back(console);
}

void Console::unregisterConsole(ConsoleRenderer *console)
{
	renderers.remove(console);
}

void Console::saveHistory()
{
	const std::string filename("history.txt");
	UserFileContext context("console");
	try {
		std::ofstream outputfile(FileOperations::expandTilde(
		        context.resolveSave(filename)).c_str());
		if (!outputfile) {
			throw FileException("Error writing Consolehistory");
		}
		std::list<std::string>::iterator it;
		for (it = history.begin(); it != history.end(); it++) {
			outputfile << it->substr(PROMPT.length()) << std::endl;
		}
	} catch (FileException &e) {
		PRT_INFO("Error while saving the consolehistory: " << filename << "\n");
	}
}

void Console::loadHistory()
{
	const std::string filename("history.txt");
	UserFileContext context("console");
	try {
		std::string line;
		std::ifstream inputfile(FileOperations::expandTilde(
		        context.resolveSave(filename)).c_str());
		if (!inputfile) {
			throw FileException("Error loading Consolehistory");
		}
		while (inputfile) {
			getline(inputfile, line);
			if (!line.empty()) {
				line.insert(0, PROMPT);
				putCommandHistory(line);
			}
		}
	} catch (FileException &e) {
		PRT_DEBUG("Error while loading the consolehistory: " << filename << "\n");
	}
}

void Console::setCursorPosition(int xPosition, int yPosition)
{
	if ((unsigned)xPosition > lines[0].length()) {
		cursorLocation.x = lines[0].length();
	} else if ((unsigned)xPosition < PROMPT.length()) {
		cursorLocation.x = (signed)PROMPT.length();
	} else {
		cursorLocation.x = xPosition;
	}
	cursorLocation.y = yPosition;
}

void Console::getCursorPosition(int *xPosition, int *yPosition)
{
	*xPosition = cursorLocation.x;
	*yPosition = cursorLocation.y;
} 

void Console::setCursorPosition(CursorXY pos)
{
	setCursorPosition(pos.x,pos.y);
}

int Console::getScrollBack()
{
	return consoleScrollBack;
}

const std::string& Console::getLine(int line)
{
	static std::string EMPTY;
	return line < lines.size() ? lines[line] : EMPTY;
}

bool Console::isVisible()
{
	return consoleSetting.getValue();
}

void Console::updateConsole()
{
	for (std::list<ConsoleRenderer*>::iterator it = renderers.begin();
	     it != renderers.end();
	     it++) {
		(*it)->updateConsole();
	}
}

void Console::setConsoleColumns(int columns)
{
	if (consoleColumns == columns) {
		return;
	}
	consoleColumns = columns;
	CircularBuffer<std::string, LINESHISTORY> linesbackup;
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
	cursorLocation.x = lines[0].length();
	cursorLocation.y = 0;
}

bool Console::signalEvent(SDL_Event &event)
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
			cursorLocation.x = PROMPT.length();
			break;
		case Keys::K_LEFT:
			combineLines(lines, lineOverflows);
			if ((unsigned)cursorPosition > PROMPT.length()) {
				cursorPosition--;
			}
			splitLines();
			break;
		case Keys::K_RIGHT:
			combineLines(lines, lineOverflows);
			if ((unsigned)cursorPosition < editLine.length()) {
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

void Console::combineLines(CircularBuffer<std::string, LINESHISTORY> &buffer,
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
		for (int i = startline; i >= startline-totallines; i--) {
			editLine += buffer[i];
		}
		for (int i = 0; i < (totallines + 1); i++) {
			buffer.removeBack();
			overflows.removeBack();
		}
	} else {
		startline = 0;
		while (((startline + totallines+1) < buffer.size()) && 
		       (overflows[startline + totallines + 1])) {
			totallines++;
		}
		for (int i = totallines; i >= 0; i--) {
			editLine += buffer[i];
		}
		for (int i = 0; i < (totallines + 1); i++) {
			buffer.removeFront();
			overflows.removeFront();
		}
	}
	
	int temp = totallines - cursorLocation.y;
	cursorPosition = (consoleColumns * temp) + cursorLocation.x;
}

void Console::splitLines()
{
	int numberOfLines = 1 + (int)(editLine.length() / consoleColumns);
	for (int i = 1; i <= numberOfLines; i++) {
		newLineConsole(editLine.substr(consoleColumns * (i - 1),
		               consoleColumns));
		lineOverflows[0] = (i != numberOfLines);
	}
	cursorLocation.x = cursorPosition % consoleColumns;
	int temp = (int)(cursorPosition / consoleColumns);
	cursorLocation.y = numberOfLines - 1 - temp;
}

void Console::printFast(const std::string &text)
{
	int end = 0;
	do {
		int start = end;
		end = text.find('\n', start);
		if (end == -1) end = text.length();
		if ((end - start) > (consoleColumns - 1)) {
			end = start + consoleColumns;
			newLineConsole(text.substr(start, end - start));
			lineOverflows[0] = true; 
		} else {
			newLineConsole(text.substr(start, end-start));
			lineOverflows[0] = false;
			end++; // skip newline
		}
	} while (end < (int)text.length());
}

void Console::printFlush()
{
	updateConsole();
}

void Console::print(const std::string &text)
{
	printFast(text);
	printFlush();
}

void Console::newLineConsole(const std::string &line)
{
	if (lines.isFull()) {
		lines.removeBack();
		lineOverflows.removeBack();	
	}
	lines.addFront(line);
	lineOverflows.addFront(false);
}

void Console::putCommandHistory(const std::string &command)
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

void Console::commandExecute()
{
	resetScrollBack();
	combineLines(lines, lineOverflows);
	putCommandHistory(editLine);
	splitLines();
	try {
		CommandController::instance()->
			executeCommand(editLine.substr(PROMPT.length()));
	} catch (CommandException &e) {
		print(e.getMessage());
	}
	putPrompt();
}

void Console::putPrompt()
{
	newLineConsole(PROMPT);
	consoleScrollBack = 0;
	commandScrollBack = history.end();
	currentLine=PROMPT;
	cursorLocation.x = PROMPT.length();
	cursorLocation.y = 0;
}

void Console::tabCompletion()
{
	resetScrollBack();
	combineLines(lines, lineOverflows);
	std::string string(editLine.substr(PROMPT.length()));
	CommandController::instance()->tabCompletion(string);
	editLine = PROMPT + string;
	cursorPosition = editLine.length();
	currentLine = editLine;
	splitLines();
}

void Console::scrollUp()
{
	if (consoleScrollBack < lines.size()) {
		consoleScrollBack++;
	}
}

void Console::scrollDown()
{
	if (consoleScrollBack > 0) {
		consoleScrollBack--;
	}
}

void Console::prevCommand()
{
	std::list<std::string>::iterator tempScrollBack = commandScrollBack;
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

void Console::nextCommand()
{
	if (commandScrollBack == history.end()) {
		return; // don't loop !
	}
	std::list<std::string>::iterator tempScrollBack = commandScrollBack;
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

void Console::clearCommand()
{
	resetScrollBack();
	combineLines(lines, lineOverflows);
	editLine = currentLine = PROMPT;
	cursorPosition = PROMPT.length();
	splitLines();
}

void Console::backspace()
{
	resetScrollBack();
	combineLines(lines, lineOverflows);
	if ((unsigned)cursorPosition > PROMPT.length()) {
		std::string temp;
		temp = editLine.substr(cursorPosition);
		editLine.erase(cursorPosition - 1);
		editLine += temp;
		cursorPosition--;
		currentLine = editLine;
	}
	splitLines();
}

void Console::delete_key()
{
	resetScrollBack();
	combineLines(lines, lineOverflows);
	if (editLine.length() > (unsigned)cursorPosition) {
		std::string temp;
		temp = editLine.substr(cursorPosition + 1);
		editLine.erase(cursorPosition);
		editLine += temp;
		currentLine = editLine;
	}
	splitLines();
}

void Console::normalKey(char chr)
{
	if (!chr) {
		return;
	}
	resetScrollBack();
	combineLines(lines, lineOverflows);
	std::string temp;
	temp += chr;
	editLine.insert(cursorPosition, temp);
	cursorPosition++;
	currentLine = editLine;
	splitLines();
}

void Console::resetScrollBack()
{
	consoleScrollBack = 0;
}
