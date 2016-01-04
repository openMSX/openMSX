#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "Command.hh"
#include "InfoTopic.hh"
#include "Setting.hh"
#include "hash_set.hh"
#include "string_ref.hh"
#include "xxhash.hh"

namespace openmsx {

class BaseSetting;
class GlobalCommandController;
class XMLElement;

/** Manages all settings.
  */
class SettingsManager
{
public:
	SettingsManager(const SettingsManager&) = delete;
	SettingsManager& operator=(const SettingsManager&) = delete;

	explicit SettingsManager(GlobalCommandController& commandController);
	~SettingsManager();

	/** Find the setting with given name.
	  * @return The requested setting or nullptr.
	  */
	BaseSetting* findSetting(string_ref name) const;
	BaseSetting* findSetting(string_ref prefix, string_ref baseName) const;

	void loadSettings(const XMLElement& config);

	void registerSetting  (BaseSetting& setting);
	void unregisterSetting(BaseSetting& setting);

private:
	BaseSetting& getByName(string_ref cmd, string_ref name) const;
	std::vector<std::string> getTabSettingNames() const;

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

	struct NameFromSetting {
		const TclObject& operator()(BaseSetting* s) const {
			return s->getFullNameObj();
		}
	};
	hash_set<BaseSetting*, NameFromSetting, XXTclHasher> settings;
};

} // namespace openmsx

#endif
