#include "TclCallback.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "StringSetting.hh"
#include "memory.hh"
#include <iostream>

using std::string;

namespace openmsx {

TclCallback::TclCallback(
		CommandController& controller,
		string_ref name,
		string_ref description,
		bool useCliComm_,
		bool save)
	: callbackSetting2(make_unique<StringSetting>(
		controller, name, description, string_ref{},
		save ? Setting::SAVE : Setting::DONT_SAVE))
	, callbackSetting(*callbackSetting2)
	, useCliComm(useCliComm_)
{
}

TclCallback::TclCallback(StringSetting& setting)
	: callbackSetting(setting)
	, useCliComm(true)
{
}

TclCallback::~TclCallback() = default;

TclObject TclCallback::getValue() const
{
	return getSetting().getValue();
}

TclObject TclCallback::execute()
{
	const auto& callback = getValue();
	if (callback.empty()) return TclObject();

	TclObject command;
	command.addListElement(callback);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1)
{
	const auto& callback = getValue();
	if (callback.empty()) return TclObject();

	TclObject command;
	command.addListElement(callback);
	command.addListElement(arg1);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1, int arg2)
{
	const auto& callback = getValue();
	if (callback.empty()) return TclObject();

	TclObject command;
	command.addListElement(callback);
	command.addListElement(arg1);
	command.addListElement(arg2);
	return executeCommon(command);
}

TclObject TclCallback::execute(int arg1, string_ref arg2)
{
	const auto& callback = getValue();
	if (callback.empty()) return TclObject();

	TclObject command;
	command.addListElement(callback);
	command.addListElement(arg1);
	command.addListElement(arg2);
	return executeCommon(command);
}

TclObject TclCallback::execute(string_ref arg1, string_ref arg2)
{
	const auto& callback = getValue();
	if (callback.empty()) return TclObject();

	TclObject command;
	command.addListElement(callback);
	command.addListElement(arg1);
	command.addListElement(arg2);
	return executeCommon(command);
}

TclObject TclCallback::executeCommon(TclObject& command)
{
	try {
		return command.executeCommand(callbackSetting.getInterpreter());
	} catch (CommandException& e) {
		string message =
			"Error executing callback function \"" +
			getSetting().getFullName() + "\": " + e.getMessage();
		if (useCliComm) {
			getSetting().getCommandController().getCliComm().printWarning(
				message);
		} else {
			std::cerr << message << std::endl;
		}
		return TclObject();
	}
}

} // namespace openmsx
