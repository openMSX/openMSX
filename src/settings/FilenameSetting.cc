#include "FilenameSetting.hh"
#include "Completer.hh"
#include "FileContext.hh"

namespace openmsx {

FilenameSetting::FilenameSetting(
		CommandController& commandController,
		string_ref name, string_ref description,
		string_ref initialValue)
	: Setting(commandController, name, description,
	          initialValue.str(), Setting::SAVE)
{
	init();
}

string_ref FilenameSetting::getTypeString() const
{
	return "filename";
}

void FilenameSetting::tabCompletion(std::vector<std::string>& tokens) const
{
	Completer::completeFileName(tokens, SystemFileContext());
}

} // namespace openmsx
