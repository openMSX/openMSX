#include "InfoTopic.hh"

#include "InfoCommand.hh"

namespace openmsx {

InfoTopic::InfoTopic(InfoCommand& infoCommand_, const std::string& name_)
	: Completer(name_)
	, infoCommand(infoCommand_)
{
	infoCommand.registerTopic(*this);
}

InfoTopic::~InfoTopic()
{
	infoCommand.unregisterTopic(*this);
}

void InfoTopic::tabCompletion(std::vector<std::string>& /*tokens*/) const
{
	// do nothing
}

Interpreter& InfoTopic::getInterpreter() const
{
	return infoCommand.getInterpreter();
}

} // namespace openmsx
