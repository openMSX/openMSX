#ifndef PROXYCOMMAND_HH
#define PROXYCOMMAND_HH

#include "Command.hh"
#include "string_ref.hh"

namespace openmsx {

class Reactor;

class ProxyCmd : public Command
{
public:
	ProxyCmd(Reactor& reactor, std::string name_);
	virtual void execute(const std::vector<TclObject>& tokens,
	                     TclObject& result);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
private:
	Command* getMachineCommand() const;
	Reactor& reactor;
	const std::string name;
};

} // namespace openmsx

#endif
