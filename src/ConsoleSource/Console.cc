// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Addapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include "Console.hh"
#include "openmsx.hh"
#include "CommandController.hh"


const std::string Console::PROMPT("> ");


Console::Console()
{
	putPrompt();
}

Console::~Console()
{
	PRT_DEBUG("Destroying a Console object");
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
		PRT_DEBUG(lines[0]);
	} while (end < (int)text.length());
	updateConsole();
}

void Console::newLineConsole(const std::string &line)
{
	if (lines.isFull())
		lines.removeBack();
	lines.addFront(line);
}

void Console::putCommandHistory(const std::string &command)
{
	if (history.isFull())
		history.removeBack();
	history.addFront(command);
}

void Console::commandExecute()
{
	putCommandHistory(lines[0]);
	try {
		CommandController::instance()->
			executeCommand(lines[0].substr(PROMPT.length()));
	} catch (CommandException &e) {
		print(e.desc);
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
