#ifndef TCLCALLBACKMESSAGES_HH
#define TCLCALLBACKMESSAGES_HH

#include "CliListener.hh"
#include "TclCallback.hh"

namespace openmsx {

class GlobalCliComm;
class CommandController;

class TclCallbackMessages final : public CliListener
{
public:
	TclCallbackMessages(GlobalCliComm& cliComm, CommandController& controller);
	~TclCallbackMessages() override;

	void log(CliComm::LogLevel level, std::string_view message) noexcept override;

	void update(CliComm::UpdateType type, std::string_view machine,
	            std::string_view name, std::string_view value) noexcept override;

private:
	GlobalCliComm& cliComm;
	TclCallback messageCallback;
};

} // namespace openmsx

#endif
