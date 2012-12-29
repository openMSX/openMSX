// $Id$

#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "StringMap.hh"
#include "string_ref.hh"
#include "noncopyable.hh"
#include <vector>
#include <memory>

namespace openmsx {

class Setting;
class GlobalCommandController;
class XMLElement;
class SettingInfo;
class SetCompleter;
class SettingCompleter;

/** Manages all settings.
  */
class SettingsManager : private noncopyable
{
public:
	explicit SettingsManager(GlobalCommandController& commandController);
	~SettingsManager();

	/** Get a setting by specifying its name.
	  * @return The Setting with the given name,
	  *   or nullptr if there is no such Setting.
	  */
	Setting* getByName(string_ref name) const;

	void loadSettings(const XMLElement& config);
	void saveSettings(XMLElement& config) const;

	void registerSetting  (Setting& setting, string_ref name);
	void unregisterSetting(Setting& setting, string_ref name);
	Setting* findSetting(string_ref name) const;

private:
	Setting& getByName(string_ref cmd, string_ref name) const;

	friend class SettingInfo;
	friend class SetCompleter;
	friend class SettingCompleter;
	const std::unique_ptr<SettingInfo>      settingInfo;
	const std::unique_ptr<SetCompleter>     setCompleter;
	const std::unique_ptr<SettingCompleter> incrCompleter;
	const std::unique_ptr<SettingCompleter> unsetCompleter;

	StringMap<Setting*> settingsMap;
};

} // namespace openmsx

#endif
