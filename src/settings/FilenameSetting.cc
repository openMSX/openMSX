// $Id$

#include "FilenameSetting.hh"
#include "CommandController.hh"
#include "FileContext.hh"

namespace openmsx {

void FilenameSettingPolicy::tabCompletion(std::vector<std::string>& tokens) const
{
	SystemFileContext context;
	CommandController::completeFileName(tokens, context);
}


FilenameSetting::FilenameSetting(const std::string& name, const std::string& description,
                                 const std::string& initialValue)
	: SettingImpl<FilenameSettingPolicy>(name, description, initialValue,
	                                     Setting::SAVE)
{
}

} // namespace openmsx
