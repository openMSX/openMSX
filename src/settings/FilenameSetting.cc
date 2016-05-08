#include "FilenameSetting.hh"
#include "Completer.hh"
#include "FileContext.hh"

namespace openmsx {

FilenameSetting::FilenameSetting(
		CommandController& commandController_,
		string_ref name_, string_ref description_,
		string_ref initialValue)
	: Setting(commandController_, name_, description_,
	          TclObject(initialValue), Setting::SAVE)
{
	init();
}

string_ref FilenameSetting::getTypeString() const
{
	return "filename";
}

void FilenameSetting::tabCompletion(std::vector<std::string>& tokens) const
{
	Completer::completeFileName(tokens, systemFileContext());
}

} // namespace openmsx
