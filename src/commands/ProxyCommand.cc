#include "ProxyCommand.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "CommandException.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "checked_cast.hh"

using std::vector;
using std::string;

namespace openmsx {

ProxyCmd::ProxyCmd(Reactor& reactor_, std::string_view name_)
	: Command(reactor_.getGlobalCommandController(), name_)
	, reactor(reactor_)
{
}

Command* ProxyCmd::getMachineCommand() const
{
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) return nullptr;
	return motherBoard->getMSXCommandController().findCommand(getName());
}

void ProxyCmd::execute(span<const TclObject> tokens, TclObject& result)
{
	if (Command* command = getMachineCommand()) {
		if (!command->isAllowedInEmptyMachine()) {
			auto* controller = checked_cast<MSXCommandController*>(
				&command->getCommandController());
			if (!controller->getMSXMotherBoard().getMachineConfig()) {
				throw CommandException(
					"Can't execute command in empty machine");
			}
		}
		command->execute(tokens, result);
	} else {
		throw CommandException("Invalid command name \"", getName(), '"');
	}
}

string ProxyCmd::help(span<const TclObject> tokens) const
{
	if (Command* command = getMachineCommand()) {
		return command->help(tokens);
	} else {
		return "unknown command: " + getName();
	}
}

void ProxyCmd::tabCompletion(vector<string>& tokens) const
{
	if (Command* command = getMachineCommand()) {
		command->tabCompletion(tokens);
	}
}

} // namespace openmsx
