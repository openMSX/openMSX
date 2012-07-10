// $Id$

#ifndef MESSAGECOMMAND_HH
#define MESSAGECOMMAND_HH

#include "Command.hh"

namespace openmsx {

class CommandController;

class MessageCommand : public Command
{
public:
	MessageCommand(CommandController& controller);

	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
};

} // namespace openmsx

#endif
