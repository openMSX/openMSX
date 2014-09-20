#ifndef TCLCALLBACKMESSAGES_HH
#define TCLCALLBACKMESSAGES_HH

#include "CliListener.hh"
#include <memory>

namespace openmsx {

class GlobalCliComm;
class CommandController;
class TclCallback;

class TclCallbackMessages final : public CliListener
{
public:
	TclCallbackMessages(GlobalCliComm& cliComm, CommandController& controller);
	~TclCallbackMessages();

	virtual void log(CliComm::LogLevel level, string_ref message);

	virtual void update(CliComm::UpdateType type, string_ref machine,
	                    string_ref name, string_ref value);

private:
	GlobalCliComm& cliComm;
	const std::unique_ptr<TclCallback> messageCallback;
};

} // namespace openmsx

#endif
