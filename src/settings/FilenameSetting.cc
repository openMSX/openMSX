// $Id$

#include "FilenameSetting.hh"
#include "Completer.hh"
#include "FileContext.hh"

namespace openmsx {

FilenameSettingPolicy::FilenameSettingPolicy()
{
}

void FilenameSettingPolicy::tabCompletion(std::vector<std::string>& tokens) const
{
	SystemFileContext context;
	CommandController* controller = NULL; // ok for SystemFileContext
	Completer::completeFileName(*controller, tokens, context);
}

std::string FilenameSettingPolicy::getTypeString() const
{
	return "filename";
}


FilenameSetting::FilenameSetting(
		CommandController& commandController,
		const std::string& name, const std::string& description,
		const std::string& initialValue)
	: SettingImpl<FilenameSettingPolicy>(
		commandController, name, description, initialValue,
		Setting::SAVE)
{
}

FilenameSetting::FilenameSetting(
		CommandController& commandController,
		const char* name, const char* description,
		const char* initialValue)
	: SettingImpl<FilenameSettingPolicy>(
		commandController, name, description, initialValue,
		Setting::SAVE)
{
}

} // namespace openmsx
