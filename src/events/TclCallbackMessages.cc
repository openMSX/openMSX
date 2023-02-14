#include "TclCallbackMessages.hh"
#include "GlobalCliComm.hh"

namespace openmsx {

TclCallbackMessages::TclCallbackMessages(GlobalCliComm& cliComm_,
                                         CommandController& controller)
	: cliComm(cliComm_)
	, messageCallback(
		controller, "message_callback",
		"Tcl proc called when a new message is available",
		"default_message_callback",
		Setting::SaveSetting::SAVE, // the user must be able to override
		false) // don't print callback err on cliComm (would cause infinite loop)
{
	cliComm.addListener(std::unique_ptr<CliListener>(this)); // wrap in unique_ptr
}

TclCallbackMessages::~TclCallbackMessages()
{
	std::unique_ptr<CliListener> ptr = cliComm.removeListener(*this);
	(void)ptr.release();
}

void TclCallbackMessages::log(CliComm::LogLevel level, std::string_view message) noexcept
{
	auto levelStr = CliComm::getLevelStrings();
	try {
		messageCallback.execute(message, levelStr[level]);
	} catch (TclObject& command) {
		// Command for this message could not be executed yet.
		// Buffer until we can redo them.
		postponedCommands.push_back(command);
	}
}

void TclCallbackMessages::update(
	CliComm::UpdateType /*type*/, std::string_view /*machine*/,
	std::string_view /*name*/, std::string_view /*value*/) noexcept
{
	// ignore
}

void TclCallbackMessages::redoPostponedCallbacks()
{
	for (auto& command: postponedCommands) {
		messageCallback.executeCommon(command);
	}
	postponedCommands.clear();
}

} // namespace openmsx
