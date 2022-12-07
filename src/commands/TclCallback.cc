#include "TclCallback.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include <iostream>
#include <memory>

namespace openmsx {

TclCallback::TclCallback(
		CommandController& controller,
		std::string_view name,
		static_string_view description,
		std::string_view defaultValue,
		Setting::SaveSetting saveSetting,
		bool useCliComm_)
	: callbackSetting2(std::in_place,
		controller, name, description, defaultValue,
		saveSetting)
	, callbackSetting(*callbackSetting2)
	, useCliComm(useCliComm_)
{
}

TclCallback::TclCallback(StringSetting& setting)
	: callbackSetting(setting)
	, useCliComm(true)
{
}

TclObject TclCallback::getValue() const
{
	return getSetting().getValue();
}

TclObject TclCallback::execute()
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1)
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback, arg1);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1, int arg2)
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback, arg1, arg2);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1, std::string_view arg2)
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback, arg1, arg2);
	return executeCommon(command);
}

TclObject TclCallback::execute(std::string_view arg1, std::string_view arg2)
{
	const auto& callback = getValue();
	if (callback.empty()) return {};

	auto command = makeTclList(callback, arg1, arg2);
	return executeCommon(command);
}

TclObject TclCallback::executeCommon(TclObject& command)
{
	try {
		return command.executeCommand(callbackSetting.getInterpreter());
	} catch (CommandException& e) {
		auto message = strCat(
			"Error executing callback function \"",
			getSetting().getFullName(), "\": ", e.getMessage());
		if (useCliComm) {
			getSetting().getCommandController().getCliComm().printWarning(
				message);
		} else {
			std::cerr << message << '\n';
		}
		return {};
	}
}

} // namespace openmsx
