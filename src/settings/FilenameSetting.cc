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
	Completer::completeFileName(tokens, SystemFileContext());
}

string_ref FilenameSettingPolicy::getTypeString() const
{
	return "filename";
}


FilenameSetting::FilenameSetting(
		CommandController& commandController,
		string_ref name, string_ref description,
		string_ref initialValue)
	: SettingImpl<FilenameSettingPolicy>(
		commandController, name, description, initialValue.str(),
		Setting::SAVE)
{
}

} // namespace openmsx
