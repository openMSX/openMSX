// $Id$

#include "Completer.hh"
#include "CommandController.hh"

using std::vector;
using std::string;
using std::set;

namespace openmsx {

Completer::Completer(CommandController& commandController_,
                     const string& name_)
	: commandController(commandController_)
	, name(name_)
{
}

Completer::~Completer()
{
}

CommandController& Completer::getCommandController() const
{
	return commandController;
}

const std::string& Completer::getName() const
{
	return name;
}

void Completer::completeString(
	vector<string>& tokens, set<string>& set, bool caseSensitive) const
{
	commandController.completeString(tokens, set, caseSensitive);
}

void Completer::completeFileName(std::vector<std::string>& tokens) const
{
	commandController.completeFileName(tokens);
}

void Completer::completeFileName(
	vector<string>& tokens, const FileContext& context,
	const set<string>& extra) const
{
	commandController.completeFileName(tokens, context, extra);
}

} // namespace openmsx
