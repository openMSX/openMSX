// $Id$

#include "Console.hh"
#include "OSDConsoleRenderer.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "Keys.hh"
#include "MSXConfig.hh"

// class ConsoleSetting

Console::ConsoleSetting::ConsoleSetting(Console *console_)
	: BooleanSetting("console", "turns console display on/off", false),
	  console(console_)
{
}

bool Console::ConsoleSetting::checkUpdate(bool newValue)
{
	console->updateConsole();
	if (newValue)
		SDL_EnableKeyRepeat (SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
	else
		SDL_EnableKeyRepeat (0,0);
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

	cursorPosition=2;
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

int Console::setCursorPosition(int position)
{
	cursorPosition = position;
	if ((unsigned)position > lines[0].length()) 
		cursorPosition = lines[0].length();
	if ((unsigned)position < PROMPT.length()) cursorPosition = (signed)PROMPT.length();
	return cursorPosition;
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
			cursorPosition=2;
			break;
		case Keys::K_LEFT:
			if (cursorPosition > PROMPT.length()) cursorPosition--;
			break;
		case Keys::K_RIGHT:
			if (cursorPosition<lines[0].length()) cursorPosition++;
			break;
		case Keys::K_HOME:
			cursorPosition=PROMPT.length();
			break;
		case Keys::K_END:
			cursorPosition=lines[0].length();
			break;
		case Keys::K_A:
			switch(modifier)
			{
			case KMOD_LCTRL:
			case KMOD_RCTRL:
				cursorPosition=PROMPT.length();
				break;
			default:
				normalKey((char)event.key.keysym.unicode);
				break;				
			}			
			break;	
		case Keys::K_E:
			switch(modifier)
			{
			case KMOD_LCTRL:
			case KMOD_RCTRL:
				cursorPosition=lines[0].length();
				break;
			default:
				normalKey((char)event.key.keysym.unicode);
				break;				
			}			
			break;	
		default:
			normalKey((char)event.key.keysym.unicode);
			
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

void Console::commandExecute()
{
	putCommandHistory(lines[0]);
	try {
		CommandController::instance()->
			executeCommand(lines[0].substr(PROMPT.length()));
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
	cursorPosition=lines[0].length();
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
		cursorPosition=lines[0].length();
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
	cursorPosition=lines[0].length();
}

void Console::backspace()
{
	if (cursorPosition > PROMPT.length())
	{
		std::string temp;
		temp=lines[0].substr(cursorPosition);
		lines[0].erase(cursorPosition-1);
		lines[0] += temp;
		cursorPosition--;
	}
}

void Console::delete_key()
{
	if (lines[0].length() >cursorPosition)
	{
		std::string temp;
		temp=lines[0].substr(cursorPosition+1);
		lines[0].erase(cursorPosition);
		lines[0] += temp;
	}
}

void Console::normalKey(char chr)
{
	if (!chr) return;
	std::string temp="";
	temp+=chr;
	
	if (lines[0].length() < (unsigned)(consoleColumns-1)) // ignore extra characters
	{
		lines[0].insert(cursorPosition,temp);
		cursorPosition++;
	}
}
