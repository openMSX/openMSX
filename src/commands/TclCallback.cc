#include "TclCallback.hh"
#include "TclObject.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "StringSetting.hh"
#include "memory.hh"
#include <iostream>

namespace openmsx {

TclCallback::TclCallback(
		CommandController& controller,
		string_ref name,
		string_ref description,
		bool useCliComm_,
		bool save)
	: callbackSetting2(make_unique<StringSetting>(
		controller, name, description, "",
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

TclCallback::~TclCallback()
{
}

string_ref TclCallback::getValue() const
{
	return getSetting().getString();
}

void TclCallback::execute()
{
	string_ref callback = getValue();
	if (callback.empty()) return;

	TclObject command({callback});
	executeCommon(command);
}

void TclCallback::execute(int arg1, int arg2)
{
	string_ref callback = getValue();
	if (callback.empty()) return;

	TclObject command({callback});
	command.addListElements({arg1, arg2});
	executeCommon(command);
}

void TclCallback::execute(int arg1, string_ref arg2)
{
	string_ref callback = getValue();
	if (callback.empty()) return;

	TclObject command({callback});
	command.addListElement(arg1);
	command.addListElement(arg2);
	executeCommon(command);
}

void TclCallback::execute(string_ref arg1, string_ref arg2)
{
	string_ref callback = getValue();
	if (callback.empty()) return;

	TclObject command({callback, arg1, arg2});
	executeCommon(command);
}

void TclCallback::executeCommon(TclObject& command)
{
	try {
		command.executeCommand(callbackSetting.getInterpreter());
	} catch (CommandException& e) {
		std::string message =
			"Error executing callback function \"" +
			getSetting().getName() + "\": " + e.getMessage();
		if (useCliComm) {
			getSetting().getCommandController().getCliComm().printWarning(
				message);
		} else {
			std::cerr << message << std::endl;
		}
	}
}

} // namespace openmsx
