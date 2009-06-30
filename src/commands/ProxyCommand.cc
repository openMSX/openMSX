// $Id$

#include "ProxyCommand.hh"
#include "MSXCommandController.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"

using std::vector;
using std::string;

namespace openmsx {

ProxyCmd::ProxyCmd(CommandController& controller, Reactor& reactor_)
	: Command(controller, "")
	, reactor(reactor_)
{
}

Command* ProxyCmd::getMachineCommand(const string& name) const
{
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return NULL;
	return motherBoard->getMSXCommandController().findCommand(name);
}

void ProxyCmd::execute(const vector<TclObject*>& tokens, TclObject& result)
{
	string name = tokens[0]->getString();
	if (Command* command = getMachineCommand(name)) {
		command->execute(tokens, result);
	} else {
		throw CommandException("Invalid command name \"" + name + '"');
	}
}

string ProxyCmd::help(const vector<string>& tokens) const
{
	if (Command* command = getMachineCommand(tokens[0])) {
		return command->help(tokens);
	} else {
		return "unknown command: " + tokens[0];
	}
}

void ProxyCmd::tabCompletion(vector<string>& tokens) const
{
	if (Command* command = getMachineCommand(tokens[0])) {
		command->tabCompletion(tokens);
	}
}

} // namespace openmsx
