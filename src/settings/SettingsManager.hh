#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "Command.hh"
#include "InfoTopic.hh"
#include "hash_map.hh"
#include "string_ref.hh"
#include "noncopyable.hh"
#include "xxhash.hh"

namespace openmsx {

class BaseSetting;
class GlobalCommandController;
class XMLElement;

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

	struct SettingInfo final : InfoTopic {
		SettingInfo(InfoCommand& openMSXInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} settingInfo;

	struct SetCompleter final : CommandCompleter {
		SetCompleter(CommandController& commandController);
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} setCompleter;

	class SettingCompleter final : public CommandCompleter {
	public:
		SettingCompleter(CommandController& commandController,
				 SettingsManager& manager,
				 const std::string& name);
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		SettingsManager& manager;
	};
	SettingCompleter incrCompleter;
	SettingCompleter unsetCompleter;

	// TODO refactor so that we can use Setting::getName()
	hash_map<std::string, BaseSetting*, XXHasher> settingsMap;
};

} // namespace openmsx

#endif
