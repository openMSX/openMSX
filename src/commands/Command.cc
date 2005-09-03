// $Id$

#include "Command.hh"
#include "CommandController.hh"
#include "TclObject.hh"

using std::vector;
using std::string;
using std::set;

namespace openmsx {

CommandCompleter::CommandCompleter(CommandController& commandController_,
                                   const string& name_)
	: commandController(commandController_)
	, name(name_)
{
	getCommandController().registerCompleter(*this, name);
}

CommandCompleter::~CommandCompleter()
{
	getCommandController().unregisterCompleter(*this, name);
}

CommandController& CommandCompleter::getCommandController() const
{
	return commandController;
}

const std::string& CommandCompleter::getName() const
{
	return name;
}

void CommandCompleter::completeString(
	vector<string>& tokens, set<string>& set, bool caseSensitive) const
{
	commandController.completeString(tokens, set, caseSensitive);
}

void CommandCompleter::completeFileName(std::vector<std::string>& tokens) const
{
	commandController.completeFileName(tokens);
}

void CommandCompleter::completeFileName(
	vector<string>& tokens, const FileContext& context,
	const set<string>& extra) const
{
	commandController.completeFileName(tokens, context, extra);
}

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
