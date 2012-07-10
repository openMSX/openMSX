// $Id$

#include "TclCallbackMessages.hh"
#include "GlobalCliComm.hh"
#include "TclCallback.hh"

using std::string;

namespace openmsx {

TclCallbackMessages::TclCallbackMessages(GlobalCliComm& cliComm_,
                                         CommandController& controller)
	: cliComm(cliComm_)
	, messageCallback(new TclCallback(controller,
		"message_callback",
		"Tcl proc called when a new message is available",
		false, // don't print callback err on cliComm (would cause infinite loop)
		false)) // don't save setting
{
	cliComm.addListener(this);
}

TclCallbackMessages::~TclCallbackMessages()
{
	cliComm.removeListener(this);
}

void TclCallbackMessages::log(CliComm::LogLevel level, string_ref message)
{
	const char* const* levelStr = CliComm::getLevelStrings();
	messageCallback->execute(message, levelStr[level]);
}

void TclCallbackMessages::update(
	CliComm::UpdateType /*type*/, string_ref /*machine*/,
	string_ref /*name*/, string_ref /*value*/)
{
	// ignore
}

} // namespace openmsx
