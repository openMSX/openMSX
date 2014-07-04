#include "ProxyCommand.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "checked_cast.hh"

using std::vector;
using std::string;

namespace openmsx {

ProxyCmd::ProxyCmd(Reactor& reactor_, std::string name_)
	: Command(reactor_.getGlobalCommandController(), "")
	, reactor(reactor_)
	, name(std::move(name_))
{
}

Command* ProxyCmd::getMachineCommand() const
{
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return nullptr;
	return motherBoard->getMSXCommandController().findCommand(name);
}

void ProxyCmd::execute(const vector<TclObject>& tokens, TclObject& result)
{
	if (Command* command = getMachineCommand()) {
		if (!command->isAllowedInEmptyMachine()) {
			auto controller = checked_cast<MSXCommandController*>(
				&command->getCommandController());
			if (!controller->getMSXMotherBoard().getMachineConfig()) {
				throw CommandException(
					"Can't execute command in empty machine");
			}
		}
		command->execute(tokens, result);
	} else {
		throw CommandException("Invalid command name \"" + name + '"');
	}
}

string ProxyCmd::help(const vector<string>& tokens) const
{
	if (Command* command = getMachineCommand()) {
		return command->help(tokens);
	} else {
		return "unknown command: " + name;
	}
}

void ProxyCmd::tabCompletion(vector<string>& tokens) const
{
	if (Command* command = getMachineCommand()) {
		command->tabCompletion(tokens);
	}
}

} // namespace openmsx
