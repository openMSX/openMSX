#include "Command.hh"
#include "CommandController.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "checked_cast.hh"

using std::vector;
using std::string;

namespace openmsx {

// class CommandCompleter

CommandCompleter::CommandCompleter(CommandController& commandController_,
                                   string_ref name_)
	: Completer(name_)
	, commandController(commandController_)
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
	if (auto globalCommandController =
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

Command::Command(CommandController& commandController_, string_ref name_)
	: CommandCompleter(commandController_, name_)
	, allowInEmptyMachine(true)
	, token(nullptr)
{
	getCommandController().registerCommand(*this, getName());
}

Command::~Command()
{
	getCommandController().unregisterCommand(*this, getName());
}

void Command::tabCompletion(vector<string>& /*tokens*/) const
{
	// do nothing
}

} // namespace openmsx
