// $Id$

#include "Console.hh"
#include "ConsoleRenderer.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "Keys.hh"


// class ConsoleSetting

Console::ConsoleSetting::ConsoleSetting(Console *console_)
	: BooleanSetting("console", "turns console display on/off", false),
	  console(console_)
{
}

bool Console::ConsoleSetting::checkUpdate(bool newValue)
{
	console->updateConsole();
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
}

Console::~Console()
{
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


bool Console::signalEvent(SDL_Event &event, const EmuTime &time)
{
	if (!isVisible())
		return true;
	if (event.type == SDL_KEYUP)
		return false;	// don't pass event to MSX-Keyboard
	
	Keys::KeyCode key = (Keys::KeyCode)event.key.keysym.sym;
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
		case Keys::K_TAB:
			tabCompletion();
			break;
		case Keys::K_RETURN:
			commandExecute(time);
			break;
		default:
			if (event.key.keysym.unicode) {
				normalKey((char)event.key.keysym.unicode);
			}
	}
	updateConsole();
	return false;	// don't pass event to MSX-Keyboard
}

void Console::print(const std::string &text)
{
	int end = 0;
	do {
		int start = end;
		end = text.find('\n', start);
		if (end == -1) end = text.length();
		newLineConsole(text.substr(start, end-start));
		end++; // skip newline
	} while (end < (int)text.length());
	updateConsole();
}

void Console::newLineConsole(const std::string &line)
{
	if (lines.isFull()) lines.removeBack();
	lines.addFront(line);
}

void Console::putCommandHistory(const std::string &command)
{
	if (history.isFull()) history.removeBack();
	history.addFront(command);
}

void Console::commandExecute(const EmuTime &time)
{
	putCommandHistory(lines[0]);
	try {
		CommandController::instance()->
			executeCommand(lines[0].substr(PROMPT.length()), time);
	} catch (CommandException &e) {
		print(e.getMessage());
	}
	putPrompt();
}

void Console::putPrompt()
{
	newLineConsole(PROMPT);
	consoleScrollBack = 0;
	commandScrollBack = -1;
}

void Console::tabCompletion()
{
	std::string string(lines[0].substr(PROMPT.length()));
	CommandController::instance()->tabCompletion(string);
	lines[0] = PROMPT + string;
}

void Console::scrollUp()
{
	if (consoleScrollBack < lines.size())
		consoleScrollBack++;
}

void Console::scrollDown()
{
	if (consoleScrollBack > 0)
		consoleScrollBack--;
}

void Console::prevCommand()
{
	if (commandScrollBack+1 < history.size()) {
		// move back a line in the command strings and copy
		// the command to the current input string
		commandScrollBack++;
		lines[0] = history[commandScrollBack];
	}
}

void Console::nextCommand()
{
	if (commandScrollBack > 0) {
		// move forward a line in the command strings and copy
		// the command to the current input string
		commandScrollBack--;
		lines[0] = history[commandScrollBack];
	} else if (commandScrollBack == 0) {
		commandScrollBack = -1;
		lines[0] = PROMPT;
	}
}

void Console::backspace()
{
	if (lines[0].length() > (unsigned)PROMPT.length())
		lines[0].erase(lines[0].length()-1);
}

void Console::normalKey(char chr)
{
	lines[0] += chr;
}
