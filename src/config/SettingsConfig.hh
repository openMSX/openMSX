// $Id$

#ifndef __SETTINGSCONFIG_HH__
#define __SETTINGSCONFIG_HH__

#include "MSXConfig.hh"
#include "Command.hh"

namespace openmsx {

class SettingsConfig : public MSXConfig
{
public:
	static SettingsConfig& instance();

	void loadSetting(FileContext& context, const string& filename);
	void saveSetting(const string& filename = "");

private:
	SettingsConfig();
	~SettingsConfig();

	string saveName;

	// SaveSettings command
	class SaveSettingsCommand : public SimpleCommand {
	public:
		SaveSettingsCommand(SettingsConfig& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help   (const vector<string>& tokens) const;
		virtual void tabCompletion(vector<string>& tokens) const;
	private:
		SettingsConfig& parent;
	} saveSettingsCommand;
};

} // namespace openmsx

#endif
