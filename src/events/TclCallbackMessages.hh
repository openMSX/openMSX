// $Id$

#ifndef TCLCALLBACKMESSAGES_HH
#define TCLCALLBACKMESSAGES_HH

#include "CliListener.hh"
#include <memory>

namespace openmsx {

class GlobalCliComm;
class CommandController;
class StringSetting;

class TclCallbackMessages : public CliListener
{
public:
	TclCallbackMessages(GlobalCliComm& cliComm, CommandController& controller);
	virtual ~TclCallbackMessages();

	virtual void log(CliComm::LogLevel level, const std::string& message);

	virtual void update(CliComm::UpdateType type, const std::string& machine,
	                    const std::string& name, const std::string& value);

private:
	GlobalCliComm& cliComm;
	const std::auto_ptr<StringSetting> messageCallbackSetting;
};

} // namespace openmsx

#endif
