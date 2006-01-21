// $Id$

#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "noncopyable.hh"
#include <map>
#include <set>
#include <string>
#include <memory>

namespace openmsx {

class Setting;
class CommandController;
class XMLElement;
class SettingInfo;
class SetCompleter;
class SettingCompleter;

/** Manages all settings.
  */
class SettingsManager : private noncopyable
{
private:
	typedef std::map<std::string, Setting*> SettingsMap;
	SettingsMap settingsMap;

public:
	explicit SettingsManager(CommandController& commandController);
	~SettingsManager();

	/** Get a setting by specifying its name.
	  * @return The Setting with the given name,
	  *   or NULL if there is no such Setting.
	  */
	Setting* getByName(const std::string& name) const;

	void loadSettings(const XMLElement& config);
	void saveSettings(XMLElement& config) const;

	void registerSetting(Setting& setting);
	void unregisterSetting(Setting& setting);

private:
	template <typename T>
	void getSettingNames(std::string& result) const;

	template <typename T>
	void getSettingNames(std::set<std::string>& result) const;

	template <typename T>
	T& getByName(const std::string& cmd, const std::string& name) const;

	friend class SettingInfo;
	friend class SetCompleter;
	friend class SettingCompleter;
	const std::auto_ptr<SettingInfo>      settingInfo;
	const std::auto_ptr<SetCompleter>     setCompleter;
	const std::auto_ptr<SettingCompleter> incrCompleter;
	const std::auto_ptr<SettingCompleter> unsetCompleter;

	CommandController& commandController;
};

} // namespace openmsx

#endif
