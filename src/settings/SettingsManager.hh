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

	/** Find the setting with given name.
	  * @return The requested setting or nullptr.
	  */
	Setting* findSetting(string_ref name) const;

	void loadSettings(const XMLElement& config);
	void saveSettings(XMLElement& config) const;

	void registerSetting  (Setting& setting, string_ref name);
	void unregisterSetting(Setting& setting, string_ref name);

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
