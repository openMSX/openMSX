#ifndef SETTINGSCONFIG_HH
#define SETTINGSCONFIG_HH

#include "Command.hh"
#include "XMLElement.hh"
#include "noncopyable.hh"
#include "string_ref.hh"
#include <string>
#include <memory>

namespace openmsx {

class SettingsManager;
class FileContext;
class HotKey;
class GlobalCommandController;
class CommandController;

class SettingsConfig : private noncopyable
{
public:
	SettingsConfig(GlobalCommandController& globalCommandController,
	               HotKey& hotKey);
	~SettingsConfig();

	void loadSetting(const FileContext& context, string_ref filename);
	void saveSetting(string_ref filename = "");
	void setSaveSettings(bool save) { mustSaveSettings = save; }
	void setSaveFilename(const FileContext& context, string_ref filename);

	SettingsManager& getSettingsManager() { return *settingsManager; }
	XMLElement& getXMLElement() { return xmlElement; }

private:
	CommandController& commandController;

	class SaveSettingsCommand final : public Command {
	public:
		SaveSettingsCommand(CommandController& commandController,
				    SettingsConfig& settingsConfig);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		SettingsConfig& settingsConfig;
	} saveSettingsCommand;

	class LoadSettingsCommand final : public Command {
	public:
		LoadSettingsCommand(CommandController& commandController,
				    SettingsConfig& settingsConfig);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		SettingsConfig& settingsConfig;
	} loadSettingsCommand;

	const std::unique_ptr<SettingsManager> settingsManager;
	XMLElement xmlElement;
	HotKey& hotKey;
	std::string saveName;
	bool mustSaveSettings;
};

} // namespace openmsx

#endif
