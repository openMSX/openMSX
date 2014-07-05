#ifndef MESSAGECOMMAND_HH
#define MESSAGECOMMAND_HH

#include "Command.hh"

namespace openmsx {

class CommandController;

class MessageCommand : public Command
{
public:
	MessageCommand(CommandController& controller);

	virtual void execute(array_ref<TclObject> tokens, TclObject& result);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
};

} // namespace openmsx

#endif
