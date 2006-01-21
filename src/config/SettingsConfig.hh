// $Id$

#ifndef SETTINGSCONFIG_HH
#define SETTINGSCONFIG_HH

#include "XMLElement.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class SettingsManager;
class HotKey;
class CommandController;
class SaveSettingsCommand;
class LoadSettingsCommand;

class SettingsConfig : public XMLElement, private noncopyable
{
public:
	explicit SettingsConfig(CommandController& commandController);
	~SettingsConfig();

	void setHotKey(HotKey* hotKey); // TODO cleanup

	void loadSetting(FileContext& context, const std::string& filename);
	void saveSetting(const std::string& filename = "");
	void setSaveSettings(bool save);

	SettingsManager& getSettingsManager();

private:
	CommandController& commandController;

	const std::auto_ptr<SaveSettingsCommand> saveSettingsCommand;
	const std::auto_ptr<LoadSettingsCommand> loadSettingsCommand;

	std::auto_ptr<SettingsManager> settingsManager;
	HotKey* hotKey;
	std::string saveName;
	bool mustSaveSettings;
};

} // namespace openmsx

#endif
