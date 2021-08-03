#include "TclCallback.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include <iostream>
#include <memory>

using std::string;

namespace openmsx {

TclCallback::TclCallback(
		CommandController& controller,
		std::string_view name,
		static_string_view description,
		bool useCliComm_,
		bool save)
	: callbackSetting2(std::in_place,
		controller, name, description, std::string_view{},
		save ? Setting::SAVE : Setting::DONT_SAVE)
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
	if (callback.empty()) return TclObject();

	TclObject command = makeTclList(callback);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1)
{
	const auto& callback = getValue();
	if (callback.empty()) return TclObject();

	TclObject command = makeTclList(callback, arg1);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1, int arg2)
{
	const auto& callback = getValue();
	if (callback.empty()) return TclObject();

	TclObject command = makeTclList(callback, arg1, arg2);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1, std::string_view arg2)
{
	const auto& callback = getValue();
	if (callback.empty()) return TclObject();

	TclObject command = makeTclList(callback, arg1, arg2);
	return executeCommon(command);
}

TclObject TclCallback::execute(std::string_view arg1, std::string_view arg2)
{
	const auto& callback = getValue();
	if (callback.empty()) return TclObject();

	TclObject command = makeTclList(callback, arg1, arg2);
	return executeCommon(command);
}

TclObject TclCallback::executeCommon(TclObject& command)
{
	try {
		return command.executeCommand(callbackSetting.getInterpreter());
	} catch (CommandException& e) {
		string message = strCat(
			"Error executing callback function \"",
			getSetting().getFullName(), "\": ", e.getMessage());
		if (useCliComm) {
			getSetting().getCommandController().getCliComm().printWarning(
				message);
		} else {
			std::cerr << message << '\n';
		}
		return TclObject();
	}
}

} // namespace openmsx
