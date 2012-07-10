// $Id$

#include "TclCallback.hh"
#include "TclObject.hh"
#include "GlobalCommandController.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "StringSetting.hh"
#include <iostream>
#include <cassert>

using std::string;

namespace openmsx {

TclCallback::TclCallback(
		CommandController& controller,
		string_ref name,
		string_ref description,
		bool useCliComm_,
		bool save)
	: callbackSetting2(new StringSetting(
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

StringSetting& TclCallback::getSetting() const
{
	return callbackSetting;
}

string TclCallback::getValue() const
{
	return getSetting().getValue();
}

void TclCallback::execute()
{
	const string callback = getValue();
	if (callback.empty()) return;

	TclObject command(callbackSetting.getInterpreter());
	command.addListElement(callback);
	executeCommon(command);
}

void TclCallback::execute(int arg1, int arg2)
{
	const string callback = getValue();
	if (callback.empty()) return;

	TclObject command(callbackSetting.getInterpreter());
	command.addListElement(callback);
	command.addListElement(arg1);
	command.addListElement(arg2);
	executeCommon(command);
}

void TclCallback::execute(int arg1, string_ref arg2)
{
	const string callback = getValue();
	if (callback.empty()) return;

	TclObject command(callbackSetting.getInterpreter());
	command.addListElement(callback);
	command.addListElement(arg1);
	command.addListElement(arg2);
	executeCommon(command);
}

void TclCallback::execute(string_ref arg1, string_ref arg2)
{
	const std::string callback = getValue();
	if (callback.empty()) return;

	TclObject command(callbackSetting.getInterpreter());
	command.addListElement(callback);
	command.addListElement(arg1);
	command.addListElement(arg2);
	executeCommon(command);
}

void TclCallback::executeCommon(TclObject& command)
{
	try {
		command.executeCommand();
	} catch (CommandException& e) {
		string message =
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
