// $Id$

#include "FilenameSetting.hh"
#include "CommandController.hh"

namespace openmsx {

void FilenameSettingPolicy::tabCompletion(std::vector<std::string>& tokens) const
{
	CommandController::completeFileName(tokens);
}


FilenameSetting::FilenameSetting(const std::string& name, const std::string& description,
                                 const std::string& initialValue)
	: SettingImpl<FilenameSettingPolicy>(name, description, initialValue,
	                                     Setting::SAVE)
{
}

FileContext& FilenameSetting::getFileContext() const
{
	return xmlNode->getFileContext();
}

} // namespace openmsx
