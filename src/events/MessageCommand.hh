#ifndef MESSAGECOMMAND_HH
#define MESSAGECOMMAND_HH

#include "Command.hh"

namespace openmsx {

class CommandController;

class MessageCommand final : public Command
{
public:
	explicit MessageCommand(CommandController& controller);

	void execute(span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
};

} // namespace openmsx

#endif
