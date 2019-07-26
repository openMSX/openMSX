#ifndef SETTINGSCONFIG_HH
#define SETTINGSCONFIG_HH

#include "SettingsManager.hh"
#include "Command.hh"
#include "XMLElement.hh"
#include <string>
#include <string_view>

namespace openmsx {

class FileContext;
class HotKey;
class GlobalCommandController;
class CommandController;

class SettingsConfig
{
public:
	SettingsConfig(GlobalCommandController& globalCommandController,
	               HotKey& hotKey);
	~SettingsConfig();

	void loadSetting(const FileContext& context, std::string_view filename);
	void saveSetting(std::string_view filename = {});
	void setSaveSettings(bool save) { mustSaveSettings = save; }
	void setSaveFilename(const FileContext& context, std::string_view filename);

	SettingsManager& getSettingsManager() { return settingsManager; }
	XMLElement& getXMLElement() { return xmlElement; }

private:
	CommandController& commandController;

	struct SaveSettingsCommand final : Command {
		explicit SaveSettingsCommand(CommandController& commandController);
		void execute(span<const TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} saveSettingsCommand;

	struct LoadSettingsCommand final : Command {
		explicit LoadSettingsCommand(CommandController& commandController);
		void execute(span<const TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} loadSettingsCommand;

	SettingsManager settingsManager;
	XMLElement xmlElement;
	HotKey& hotKey;
	std::string saveName;
	bool mustSaveSettings;
};

} // namespace openmsx

#endif
