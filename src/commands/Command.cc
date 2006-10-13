// $Id$

#include "Command.hh"
#include "CommandRegistry.hh"
#include "TclObject.hh"

using std::vector;
using std::string;

namespace openmsx {

// class CommandCompleter

CommandCompleter::CommandCompleter(CommandRegistry& commandRegistry_,
                                   const string& name)
	: Completer(name)
	, commandRegistry(commandRegistry_)
{
	getCommandRegistry().registerCompleter(*this, getName());
}

CommandCompleter::~CommandCompleter()
{
	getCommandRegistry().unregisterCompleter(*this, getName());
}

CommandRegistry& CommandCompleter::getCommandRegistry() const
{
	return commandRegistry;
}


// class Command

Command::Command(CommandRegistry& commandRegistry, const string& name)
	: CommandCompleter(commandRegistry, name)
{
	getCommandRegistry().registerCommand(*this, getName());
}

Command::~Command()
{
	getCommandRegistry().unregisterCommand(*this, getName());
}

void Command::tabCompletion(vector<string>& /*tokens*/) const
{
	// do nothing
}

CommandController& Command::getCommandController() const
{
	return getCommandRegistry().getCommandController();
}


// class SimpleCommand

SimpleCommand::SimpleCommand(CommandRegistry& commandRegistry,
                             const string& name)
	: Command(commandRegistry, name)
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
