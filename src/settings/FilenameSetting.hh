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
	void setContext(const FileContext& context);
private:
	const FileContext* context;
};

class FilenameSetting : public SettingImpl<FilenameSettingPolicy>
{
public:
	FilenameSetting(const std::string& name, const std::string& description,
	                const std::string& initialValue);

	FileContext& getFileContext() const;
};

} // namespace openmsx

#endif
