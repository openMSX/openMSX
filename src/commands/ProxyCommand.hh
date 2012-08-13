// $Id$

#ifndef PROXYCOMMAND_HH
#define PROXYCOMMAND_HH

#include "Command.hh"
#include "string_ref.hh"

namespace openmsx {

class CommandController;
class Reactor;

class ProxyCmd : public Command
{
public:
	ProxyCmd(CommandController& controller, Reactor& reactor);
	virtual void execute(const std::vector<TclObject>& tokens,
	                     TclObject& result);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
private:
	Command* getMachineCommand(string_ref name) const;
	Reactor& reactor;
};

} // namespace openmsx

#endif
