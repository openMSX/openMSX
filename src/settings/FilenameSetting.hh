// $Id$

#ifndef FILENAMESETTING_HH
#define FILENAMESETTING_HH

#include "StringSetting.hh"

namespace openmsx {

class FileContext;

class FilenameSettingPolicy : public StringSettingPolicy
{
protected:
	explicit FilenameSettingPolicy();
	void tabCompletion(std::vector<std::string>& tokens) const;
	std::string getTypeString() const;
};

class FilenameSetting : public SettingImpl<FilenameSettingPolicy>
{
public:
	FilenameSetting(CommandController& commandController,
	                const std::string& name, const std::string& description,
	                const std::string& initialValue);
	FilenameSetting(CommandController& commandController,
	                const char* name, const char* description,
	                const char* initialValue);
};

} // namespace openmsx

#endif
