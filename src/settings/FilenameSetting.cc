#include "FilenameSetting.hh"
#include "Completer.hh"
#include "FileContext.hh"

namespace openmsx {

FilenameSetting::FilenameSetting(
		CommandController& commandController_,
		string_view name_, string_view description_,
		string_view initialValue)
	: Setting(commandController_, name_, description_,
	          TclObject(initialValue), Setting::SAVE)
{
	init();
}

string_view FilenameSetting::getTypeString() const
{
	return "filename";
}

void FilenameSetting::tabCompletion(std::vector<std::string>& tokens) const
{
	Completer::completeFileName(tokens, systemFileContext());
}

} // namespace openmsx
