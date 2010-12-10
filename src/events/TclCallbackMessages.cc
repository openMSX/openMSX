// $Id$

#include "TclCallbackMessages.hh"
#include "GlobalCliComm.hh"
#include "StringSetting.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include <iostream>

using std::string;

namespace openmsx {

TclCallbackMessages::TclCallbackMessages(GlobalCliComm& cliComm_,
                                         CommandController& controller)
	: cliComm(cliComm_)
	, messageCallbackSetting(new StringSetting(controller,
		"message_callback",
		"Tcl proc called when a new message is available",
		"", Setting::DONT_SAVE))
{
	cliComm.addListener(this);
}

TclCallbackMessages::~TclCallbackMessages()
{
	cliComm.removeListener(this);
}

void TclCallbackMessages::log(CliComm::LogLevel level, const string& message)
{
	// Possible optimization: listen to setting changes and only register
	// cliComm listener when value not empty (probably not worth it).
	string callback = messageCallbackSetting->getValue();
	if (callback.empty()) return;

	TclObject command(messageCallbackSetting->getInterpreter());
	command.addListElement(callback);
	command.addListElement(message);
	const char* const* levelStr = CliComm::getLevelStrings();
	command.addListElement(levelStr[level]);
	try {
		command.executeCommand();
	} catch (CommandException& e) {
		// We can't call CliComm::printWarning here,
		// it could lead to an infinite loop.
		std::cerr << "Error executing message_callback function "
		             "\"" << callback << "\": " << e.getMessage() << "\n"
		             "Please fix your script or set a proper callback "
		             "function in the message_callback setting."
		          << std::endl;
	}
}

void TclCallbackMessages::update(
	CliComm::UpdateType /*type*/, const string& /*machine*/,
	const string& /*name*/, const string& /*value*/)
{
	// ignore
}

} // namespace openmsx
