// $Id$

#include "InfoTopic.hh"
#include "CommandController.hh"
#include "InfoCommand.hh"

using std::string;
using std::vector;

namespace openmsx {

InfoTopic::InfoTopic(CommandController& commandController_, const string& name)
	: Completer(name)
	, commandController(commandController_)
{
	commandController.getInfoCommand().registerTopic(*this, getName());
}

InfoTopic::~InfoTopic()
{
	commandController.getInfoCommand().unregisterTopic(*this, getName());
}

void InfoTopic::tabCompletion(vector<string>& /*tokens*/) const
{
	// do nothing
}

} // namespace openmsx
