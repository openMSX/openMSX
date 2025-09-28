#include "Command.hh"

#include "CommandController.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"

#include "checked_cast.hh"

namespace openmsx {

// class CommandCompleter

CommandCompleter::CommandCompleter(CommandController& controller_,
                                   std::string_view name_)
	: Completer(name_)
	, commandController(controller_)
{
	getCommandController().registerCompleter(*this, getName());
}

CommandCompleter::~CommandCompleter()
{
	getCommandController().unregisterCompleter(*this, getName());
}

// TODO: getCommandController(), getGlobalCommandController() and
//       getInterpreter() occur both here and in Setting.

GlobalCommandController& CommandCompleter::getGlobalCommandController() const
{
	if (auto* globalCommandController =
	    dynamic_cast<GlobalCommandController*>(&commandController)) {
		return *globalCommandController;
	} else {
		return checked_cast<MSXCommandController*>(&commandController)
			->getGlobalCommandController();
	}
}

Interpreter& CommandCompleter::getInterpreter() const
{
	return getCommandController().getInterpreter();
}

CliComm& CommandCompleter::getCliComm() const
{
	return getCommandController().getCliComm();
}

// class Command

Command::Command(CommandController& controller_, std::string_view name_)
	: CommandCompleter(controller_, name_)
{
	getCommandController().registerCommand(*this, getName());
}

Command::~Command()
{
	getCommandController().unregisterCommand(*this, getName());
}

void Command::tabCompletion(std::vector<std::string>& /*tokens*/) const
{
	// do nothing
}

} // namespace openmsx
