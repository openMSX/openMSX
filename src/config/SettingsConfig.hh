// $Id$

#ifndef SETTINGSCONFIG_HH
#define SETTINGSCONFIG_HH

#include "XMLElement.hh"
#include "Command.hh"
#include <memory>

namespace openmsx {

class SettingsManager;

class SettingsConfig : public XMLElement
{
public:
	static SettingsConfig& instance();

	void loadSetting(FileContext& context, const std::string& filename);
	void saveSetting(const std::string& filename = "");
	void setSaveSettings(bool save);

	SettingsManager& getSettingsManager();

private:
	SettingsConfig();
	~SettingsConfig();

	// SaveSettings command
	class SaveSettingsCommand : public SimpleCommand {
	public:
		SaveSettingsCommand(SettingsConfig& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsConfig& parent;
	} saveSettingsCommand;

	// LoadSettings command
	class LoadSettingsCommand : public SimpleCommand {
	public:
		LoadSettingsCommand(SettingsConfig& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help   (const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsConfig& parent;
	} loadSettingsCommand;
	
	std::auto_ptr<SettingsManager> settingsManager;
	std::string saveName;
	bool mustSaveSettings;
};

} // namespace openmsx

#endif
