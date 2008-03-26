// $Id$

#ifndef SETTINGSCONFIG_HH
#define SETTINGSCONFIG_HH

#include "noncopyable.hh"
#include <string>
#include <memory>

namespace openmsx {

class SettingsManager;
class XMLElement;
class FileContext;
class HotKey;
class GlobalCommandController;
class CommandController;
class SaveSettingsCommand;
class LoadSettingsCommand;

class SettingsConfig : private noncopyable
{
public:
	SettingsConfig(GlobalCommandController& globalCommandController,
	               HotKey& hotKey);
	~SettingsConfig();

	void loadSetting(FileContext& context, const std::string& filename);
	void saveSetting(const std::string& filename = "");
	void setSaveSettings(bool save);
	void setSaveFilename(FileContext& context, const std::string& filename);

	SettingsManager& getSettingsManager();
	XMLElement& getXMLElement();

private:
	CommandController& commandController;

	const std::auto_ptr<SaveSettingsCommand> saveSettingsCommand;
	const std::auto_ptr<LoadSettingsCommand> loadSettingsCommand;

	std::auto_ptr<SettingsManager> settingsManager;
	std::auto_ptr<XMLElement> xmlElement;
	HotKey& hotKey;
	std::string saveName;
	bool mustSaveSettings;
};

} // namespace openmsx

#endif
