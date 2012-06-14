// $Id$

#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "StringMap.hh"
#include "noncopyable.hh"
#include <set>
#include <string>
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
private:
	typedef StringMap<Setting*> SettingsMap;
	SettingsMap settingsMap;

public:
	explicit SettingsManager(GlobalCommandController& commandController);
	~SettingsManager();

	/** Get a setting by specifying its name.
	  * @return The Setting with the given name,
	  *   or NULL if there is no such Setting.
	  */
	Setting* getByName(const std::string& name) const;

	void loadSettings(const XMLElement& config);
	void saveSettings(XMLElement& config) const;

	void registerSetting(Setting& setting, const std::string& name);
	void unregisterSetting(Setting& setting, const std::string& name);
	Setting* findSetting(const std::string& name) const;

private:
	void getSettingNames(std::set<std::string>& result) const;
	Setting& getByName(const std::string& cmd,
	                   const std::string& name) const;

	friend class SettingInfo;
	friend class SetCompleter;
	friend class SettingCompleter;
	const std::auto_ptr<SettingInfo>      settingInfo;
	const std::auto_ptr<SetCompleter>     setCompleter;
	const std::auto_ptr<SettingCompleter> incrCompleter;
	const std::auto_ptr<SettingCompleter> unsetCompleter;
};

} // namespace openmsx

#endif
