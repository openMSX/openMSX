// $Id$

#include "FilenameSetting.hh"
#include "CommandController.hh"
#include "FileContext.hh"

namespace openmsx {

FilenameSettingPolicy::FilenameSettingPolicy(CommandController& commandController)
	: StringSettingPolicy(commandController)
{
}

void FilenameSettingPolicy::tabCompletion(std::vector<std::string>& tokens) const
{
	SystemFileContext context;
	getCommandController().completeFileName(tokens, context);
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

} // namespace openmsx
