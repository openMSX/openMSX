#include "InfoTopic.hh"
#include "InfoCommand.hh"

using std::string;
using std::vector;

namespace openmsx {

InfoTopic::InfoTopic(InfoCommand& infoCommand_, const string& name_)
	: Completer(name_)
	, infoCommand(infoCommand_)
{
	infoCommand.registerTopic(*this);
}

InfoTopic::~InfoTopic()
{
	infoCommand.unregisterTopic(*this);
}

void InfoTopic::tabCompletion(vector<string>& /*tokens*/) const
{
	// do nothing
}

Interpreter& InfoTopic::getInterpreter() const
{
	return infoCommand.getInterpreter();
}

} // namespace openmsx
