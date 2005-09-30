// $Id$

#include "Command.hh"
#include "CommandController.hh"
#include "TclObject.hh"

using std::vector;
using std::string;

namespace openmsx {

// class CommandCompleter

CommandCompleter::CommandCompleter(CommandController& commandController,
                                   const string& name)
	: Completer(commandController, name)
{
	getCommandController().registerCompleter(*this, getName());
}

CommandCompleter::~CommandCompleter()
{
	getCommandController().unregisterCompleter(*this, getName());
}


// class Command

Command::Command(CommandController& commandController, const string& name)
	: CommandCompleter(commandController, name)
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


// class SimpleCommand

SimpleCommand::SimpleCommand(CommandController& commandController,
                             const string& name)
	: Command(commandController, name)
{
}

SimpleCommand::~SimpleCommand()
{
}

void SimpleCommand::execute(const vector<TclObject*>& tokens,
                            TclObject& result)
{
	vector<string> strings;
	strings.reserve(tokens.size());
	for (vector<TclObject*>::const_iterator it = tokens.begin();
	     it != tokens.end(); ++it) {
		strings.push_back((*it)->getString());
	}
	result.setString(execute(strings));
}

} // namespace openmsx
