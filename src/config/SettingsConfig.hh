// $Id$

#ifndef SETTINGSCONFIG_HH
#define SETTINGSCONFIG_HH

#include "XMLElement.hh"
#include "Command.hh"
#include <memory>

namespace openmsx {

class SettingsManager;
class HotKey;

class SettingsConfig : public XMLElement
{
public:
	SettingsConfig(CommandController& commandController);
	~SettingsConfig();

	void setHotKey(HotKey* hotKey); // TODO cleanup
	
	void loadSetting(FileContext& context, const std::string& filename);
	void saveSetting(const std::string& filename = "");
	void setSaveSettings(bool save);

	SettingsManager& getSettingsManager();

private:
	CommandController& commandController;
	
	// SaveSettings command
	class SaveSettingsCommand : public SimpleCommand {
	public:
		SaveSettingsCommand(CommandController& commandController,
		                    SettingsConfig& settingsConfig);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsConfig& settingsConfig;
	} saveSettingsCommand;

	// LoadSettings command
	class LoadSettingsCommand : public SimpleCommand {
	public:
		LoadSettingsCommand(CommandController& commandController,
		                    SettingsConfig& settingsConfig);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsConfig& settingsConfig;
	} loadSettingsCommand;
	
	std::auto_ptr<SettingsManager> settingsManager;
	HotKey* hotKey;
	std::string saveName;
	bool mustSaveSettings;
};

} // namespace openmsx

#endif
