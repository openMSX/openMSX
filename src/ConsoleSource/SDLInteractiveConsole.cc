// $Id$

#include <cassert>
#include "CommandController.hh"
#include "ConsoleManager.hh"
#include "EventDistributor.hh"
#include "SDLInteractiveConsole.hh"
#include "Keys.hh"


SDLInteractiveConsole::SDLInteractiveConsole() :
	consoleCmd(this)
{
	isVisible = false;
	
	SDL_EnableUNICODE(1);
	
	ConsoleManager::instance()->registerConsole(this);
	EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->registerEventListener(SDL_KEYUP,   this);
	CommandController::instance()->registerCommand(&consoleCmd, "console");
}

SDLInteractiveConsole::~SDLInteractiveConsole()
{
	CommandController::instance()->unregisterCommand(&consoleCmd, "console");
	EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this);
	EventDistributor::instance()->unregisterEventListener(SDL_KEYUP,   this);
	ConsoleManager::instance()->unregisterConsole(this);
}


// Takes keys from the keyboard and inputs them to the console
bool SDLInteractiveConsole::signalEvent(SDL_Event &event, const EmuTime &time)
{
	if (!isVisible)
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
			if (event.key.keysym.unicode)
				normalKey((char)event.key.keysym.unicode);
	}
	updateConsole();
	return false;	// don't pass event to MSX-Keyboard
}


// Console command
SDLInteractiveConsole::ConsoleCmd::ConsoleCmd(SDLInteractiveConsole *cons)
{
	console = cons;
}

void SDLInteractiveConsole::ConsoleCmd::execute(const std::vector<std::string> &tokens,
                                                const EmuTime &time)
{
	switch (tokens.size()) {
	case 1:
		console->isVisible = !console->isVisible;
		break;
	case 2:
		if (tokens[1] == "on") {
			console->isVisible = true;
			break;
		} 
		if (tokens[1] == "off") {
			console->isVisible = false;
			break;
		}
		// fall through
	default:
		throw CommandException("Syntax error");
	}
}

void SDLInteractiveConsole::ConsoleCmd::help(const std::vector<std::string> &tokens) const
{
	print("This command turns console display on/off");
	print(" console:     toggle console display");
	print(" console on:  show console display");
	print(" console off: remove console display");
} 
