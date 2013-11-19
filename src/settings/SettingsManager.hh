#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "StringMap.hh"
#include "string_ref.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class BaseSetting;
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
	BaseSetting* findSetting(string_ref name) const;

	void loadSettings(const XMLElement& config);

	void registerSetting  (BaseSetting& setting, string_ref name);
	void unregisterSetting(BaseSetting& setting, string_ref name);

private:
	BaseSetting& getByName(string_ref cmd, string_ref name) const;

	friend class SettingInfo;
	friend class SetCompleter;
	friend class SettingCompleter;
	const std::unique_ptr<SettingInfo>      settingInfo;
	const std::unique_ptr<SetCompleter>     setCompleter;
	const std::unique_ptr<SettingCompleter> incrCompleter;
	const std::unique_ptr<SettingCompleter> unsetCompleter;

	StringMap<BaseSetting*> settingsMap;
};

} // namespace openmsx

#endif
