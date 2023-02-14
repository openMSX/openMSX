#include "TclCallback.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "Reactor.hh"
#include "checked_cast.hh"
#include <iostream>
#include <memory>

namespace openmsx {

TclCallback::TclCallback(
		CommandController& controller,
		std::string_view name,
		static_string_view description,
		std::string_view defaultValue,
		Setting::SaveSetting saveSetting,
		bool isMessageCallback_)
	: callbackSetting2(std::in_place,
		controller, name, description, defaultValue,
		saveSetting)
	, callbackSetting(*callbackSetting2)
	, isMessageCallback(isMessageCallback_)
{
}

TclCallback::TclCallback(StringSetting& setting)
	: callbackSetting(setting)
	, isMessageCallback(false)
{
}

TclObject TclCallback::getValue() const
{
	return getSetting().getValue();
}

TclObject TclCallback::execute() const
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1) const
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback, arg1);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1, int arg2) const
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback, arg1, arg2);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1, std::string_view arg2) const
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback, arg1, arg2);
	return executeCommon(command);
}

TclObject TclCallback::execute(std::string_view arg1, std::string_view arg2) const
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback, arg1, arg2);
	return executeCommon(command);
}

TclObject TclCallback::executeCommon(TclObject& command) const
{
	try {
		return command.executeCommand(callbackSetting.getInterpreter());
	} catch (CommandException& e) {
		auto message = strCat(
			"Error executing callback function \"",
			getSetting().getFullName(), "\": ", e.getMessage());
		auto& commandController = getSetting().getCommandController();
		if (!isMessageCallback) {
			commandController.getCliComm().printWarning(message);
		} else {
			if (checked_cast<GlobalCommandController&>(commandController).getReactor().isFullyStarted()) {
				std::cerr << message << '\n';
			} else {
				// This is a message callback that cannot be
				// executed yet.
				// Let the caller deal with this.
				throw command;
			}
		}
		return {};
	}
}

} // namespace openmsx
