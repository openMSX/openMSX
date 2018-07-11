#ifndef PROXYCOMMAND_HH
#define PROXYCOMMAND_HH

#include "Command.hh"
#include "string_view.hh"

namespace openmsx {

class Reactor;

class ProxyCmd final : public Command
{
public:
	ProxyCmd(Reactor& reactor, string_view name);
	void execute(array_ref<TclObject> tokens,
	             TclObject& result) override;
	std::string help(const std::vector<std::string>& tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	Command* getMachineCommand() const;
	Reactor& reactor;
};

} // namespace openmsx

#endif
