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
	~TclCallbackMessages();

	void log(CliComm::LogLevel level, string_ref message) override;

	void update(CliComm::UpdateType type, string_ref machine,
	            string_ref name, string_ref value) override;

private:
	GlobalCliComm& cliComm;
	TclCallback messageCallback;
};

} // namespace openmsx

#endif
