// $Id$

#ifndef FILENAMESETTING_HH
#define FILENAMESETTING_HH

#include "StringSetting.hh"

namespace openmsx {

class FileContext;

class FilenameSettingPolicy : public StringSettingPolicy
{
protected:
	void tabCompletion(std::vector<std::string>& tokens) const;
};

class FilenameSetting : public SettingImpl<FilenameSettingPolicy>
{
public:
	FilenameSetting(const std::string& name, const std::string& description,
	                const std::string& initialValue);
};

} // namespace openmsx

#endif
