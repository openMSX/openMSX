// $Id$

#ifndef SETTINGSCONFIG_HH
#define SETTINGSCONFIG_HH

#include "XMLElement.hh"
#include "Command.hh"

namespace openmsx {

class BooleanSetting;

class SettingsConfig : public XMLElement
{
public:
	static SettingsConfig& instance();

	void loadSetting(FileContext& context, const std::string& filename);
	void saveSetting(const std::string& filename = "");

private:
	SettingsConfig();
	~SettingsConfig();

	std::string saveName;
	BooleanSetting* autoSaveSetting;

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
};

} // namespace openmsx

#endif
