#include "FilenameSetting.hh"

#include "Completer.hh"
#include "FileContext.hh"

namespace openmsx {

FilenameSetting::FilenameSetting(
		CommandController& commandController_,
		std::string_view name_, static_string_view description_,
		std::string_view initialValue)
	: Setting(commandController_, name_, description_,
	          TclObject(initialValue), Setting::Save::YES)
{
	init();
}

std::string_view FilenameSetting::getTypeString() const
{
	return "filename";
}

void FilenameSetting::tabCompletion(std::vector<std::string>& tokens) const
{
	Completer::completeFileName(commandController, tokens, systemFileContext());
}

} // namespace openmsx
