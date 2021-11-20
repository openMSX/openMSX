#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "Command.hh"
#include "InfoTopic.hh"
#include "Setting.hh"
#include "hash_set.hh"
#include "TclObject.hh"
#include <string_view>

namespace openmsx {

class SettingsConfig;
class GlobalCommandController;

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
	[[nodiscard]] BaseSetting* findSetting(std::string_view name) const;
	[[nodiscard]] BaseSetting* findSetting(std::string_view prefix, std::string_view baseName) const;

	void loadSettings(const SettingsConfig& config);

	void registerSetting  (BaseSetting& setting);
	void unregisterSetting(BaseSetting& setting);

private:
	[[nodiscard]] BaseSetting& getByName(std::string_view cmd, std::string_view name) const;
	[[nodiscard]] std::vector<std::string> getTabSettingNames() const;

private:
	struct SettingInfo final : InfoTopic {
		explicit SettingInfo(InfoCommand& openMSXInfoCommand);
		void execute(span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} settingInfo;

	struct SetCompleter final : CommandCompleter {
		explicit SetCompleter(CommandController& commandController);
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} setCompleter;

	class SettingCompleter final : public CommandCompleter {
	public:
		SettingCompleter(CommandController& commandController,
				 SettingsManager& manager,
				 const std::string& name);
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		SettingsManager& manager;
	};
	SettingCompleter incrCompleter;
	SettingCompleter unsetCompleter;

	struct NameFromSetting {
		[[nodiscard]] const TclObject& operator()(BaseSetting* s) const {
			return s->getFullNameObj();
		}
	};
	hash_set<BaseSetting*, NameFromSetting, XXTclHasher> settings;
};

} // namespace openmsx

#endif
