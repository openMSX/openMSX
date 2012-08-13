// $Id$

#include "Command.hh"
#include "CommandController.hh"
#include "GlobalCommandController.hh"
#include "MSXCommandController.hh"
#include "TclObject.hh"
#include "checked_cast.hh"
#include "unreachable.hh"

using std::vector;
using std::string;

namespace openmsx {

// class CommandCompleter

CommandCompleter::CommandCompleter(CommandController& commandController_,
                                   string_ref name)
	: Completer(name)
	, commandController(commandController_)
{
	if (!getName().empty()) {
		getCommandController().registerCompleter(*this, getName());
	}
}

CommandCompleter::~CommandCompleter()
{
	if (!getName().empty()) {
		getCommandController().unregisterCompleter(*this, getName());
	}
}

// TODO: getCommandController(), getGlobalCommandController() and
//       getInterpreter() occur both here and in Setting.

CommandController& CommandCompleter::getCommandController() const
{
	return commandController;
}

GlobalCommandController& CommandCompleter::getGlobalCommandController() const
{
	if (GlobalCommandController* globalCommandController =
	    dynamic_cast<GlobalCommandController*>(&commandController)) {
		return *globalCommandController;
	} else {
		return checked_cast<MSXCommandController*>(&commandController)
			->getGlobalCommandController();
	}
}

Interpreter& CommandCompleter::getInterpreter() const
{
	return getGlobalCommandController().getInterpreter();
}

CliComm& CommandCompleter::getCliComm() const
{
	return getCommandController().getCliComm();
}

// class Command

Command::Command(CommandController& commandController, string_ref name)
	: CommandCompleter(commandController, name)
	, allowInEmptyMachine(true)
{
	if (!getName().empty()) {
		getCommandController().registerCommand(*this, getName());
	}
}

Command::~Command()
{
	if (!getName().empty()) {
		getCommandController().unregisterCommand(*this, getName());
	}
}

void Command::execute(const vector<TclObject>& tokens,
                      TclObject& result)
{
	vector<string> strings;
	strings.reserve(tokens.size());
	for (vector<TclObject>::const_iterator it = tokens.begin();
	     it != tokens.end(); ++it) {
		strings.push_back(it->getString().str());
	}
	result.setString(execute(strings));
}

string Command::execute(const vector<string>& /*tokens*/)
{
	// either this method or the method above should be reimplemented
	// by the subclasses
	UNREACHABLE; return "";
}

void Command::tabCompletion(vector<string>& /*tokens*/) const
{
	// do nothing
}

} // namespace openmsx
