// $Id$

#include "InfoTopic.hh"
#include "CommandController.hh"
#include "InfoCommand.hh"

using std::string;
using std::vector;

namespace openmsx {

InfoTopic::InfoTopic(CommandController& commandController, const string& name)
	: CommandCompleter(commandController, name)
{
	getCommandController().getInfoCommand().registerTopic(*this, getName());
}

InfoTopic::~InfoTopic()
{
	getCommandController().getInfoCommand().unregisterTopic(*this, getName());
}

void InfoTopic::tabCompletion(vector<string>& /*tokens*/) const
{
	// do nothing
}

} // namespace openmsx
