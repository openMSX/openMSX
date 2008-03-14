// $Id$

#ifndef USERSETTINGS_HH
#define USERSETTINGS_HH

#include "noncopyable.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class CommandController;
class UserSettingCommand;
class Setting;

class UserSettings : private noncopyable
{
public:
	typedef std::vector<Setting*> Settings;

	explicit UserSettings(CommandController& commandController);
	~UserSettings();

	void addSetting(std::auto_ptr<Setting> setting);
	void deleteSetting(Setting& setting);
	Setting* findSetting(const std::string& name) const;
	const Settings& getSettings() const;

private:
	const std::auto_ptr<UserSettingCommand> userSettingCommand;
	Settings settings;
};

} // namespace openmsx

#endif
