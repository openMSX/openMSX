#ifndef MESSAGECOMMAND_HH
#define MESSAGECOMMAND_HH

#include "Command.hh"

namespace openmsx {

class CommandController;

class MessageCommand final : public Command
{
public:
	explicit MessageCommand(CommandController& controller);

	void execute(array_ref<TclObject> tokens, TclObject& result) override;
	std::string help(const std::vector<std::string>& tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
};

} // namespace openmsx

#endif
