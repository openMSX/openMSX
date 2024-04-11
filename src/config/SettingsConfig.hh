#ifndef SETTINGSCONFIG_HH
#define SETTINGSCONFIG_HH

#include "SettingsManager.hh"
#include "Command.hh"

#include "hash_map.hh"
#include "xxhash.hh"

#include <string>
#include <string_view>

namespace openmsx {

class FileContext;
class HotKey;
class GlobalCommandController;
class CommandController;
class Shortcuts;

class SettingsConfig
{
public:
	SettingsConfig(GlobalCommandController& globalCommandController,
	               HotKey& hotKey);
	~SettingsConfig();

	void loadSetting(const FileContext& context, std::string_view filename);
	void saveSetting(std::string filename = {});
	void setSaveSettings(bool save) { mustSaveSettings = save; }
	void setSaveFilename(const FileContext& context, std::string_view filename);

	[[nodiscard]] SettingsManager& getSettingsManager() { return settingsManager; }

	// manipulate info that would be stored in settings.xml
	[[nodiscard]] const std::string* getValueForSetting(std::string_view setting) const;
	void setValueForSetting(std::string_view setting, std::string_view value);
	void removeValueForSetting(std::string_view setting);

	void setShortcuts(Shortcuts& shortcuts_) { shortcuts = &shortcuts_; };

private:
	CommandController& commandController;
	Shortcuts* shortcuts;

	struct SaveSettingsCommand final : Command {
		explicit SaveSettingsCommand(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} saveSettingsCommand;

	struct LoadSettingsCommand final : Command {
		explicit LoadSettingsCommand(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} loadSettingsCommand;

	SettingsManager settingsManager;
	hash_map<std::string, std::string, XXHasher> settingValues;
	HotKey& hotKey;
	std::string saveName;
	bool mustSaveSettings = false;
};

} // namespace openmsx

#endif
