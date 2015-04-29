#include "TclCallbackMessages.hh"
#include "GlobalCliComm.hh"

namespace openmsx {

TclCallbackMessages::TclCallbackMessages(GlobalCliComm& cliComm_,
                                         CommandController& controller)
	: cliComm(cliComm_)
	, messageCallback(
		controller, "message_callback",
		"Tcl proc called when a new message is available",
		false, // don't print callback err on cliComm (would cause infinite loop)
		false) // don't save setting
{
	cliComm.addListener(std::unique_ptr<CliListener>(this)); // wrap in unique_ptr
}

TclCallbackMessages::~TclCallbackMessages()
{
	std::unique_ptr<CliListener> ptr = cliComm.removeListener(*this);
	ptr.release();
}

void TclCallbackMessages::log(CliComm::LogLevel level, string_ref message)
{
	auto levelStr = CliComm::getLevelStrings();
	messageCallback.execute(message, levelStr[level]);
}

void TclCallbackMessages::update(
	CliComm::UpdateType /*type*/, string_ref /*machine*/,
	string_ref /*name*/, string_ref /*value*/)
{
	// ignore
}

} // namespace openmsx
