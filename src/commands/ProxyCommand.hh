#ifndef PROXYCOMMAND_HH
#define PROXYCOMMAND_HH

#include "Command.hh"
#include <string_view>

namespace openmsx {

class Reactor;

class ProxyCmd final : public Command
{
public:
	ProxyCmd(Reactor& reactor, std::string_view name);
	void execute(span<const TclObject> tokens,
	             TclObject& result) override;
	[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	[[nodiscard]] Command* getMachineCommand() const;
	Reactor& reactor;
};

} // namespace openmsx

#endif
