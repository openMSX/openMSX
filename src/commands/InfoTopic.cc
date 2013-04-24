#include "InfoTopic.hh"
#include "InfoCommand.hh"

using std::string;
using std::vector;

namespace openmsx {

InfoTopic::InfoTopic(InfoCommand& infoCommand_, const string& name)
	: Completer(name)
	, infoCommand(infoCommand_)
{
	infoCommand.registerTopic(*this, getName());
}

InfoTopic::~InfoTopic()
{
	infoCommand.unregisterTopic(*this, getName());
}

void InfoTopic::tabCompletion(vector<string>& /*tokens*/) const
{
	// do nothing
}

} // namespace openmsx
