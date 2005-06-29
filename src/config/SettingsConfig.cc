// $Id$

#include "SettingsConfig.hh"
#include "SettingsManager.hh"
#include "XMLLoader.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"
#include "CliComm.hh"
#include "BooleanSetting.hh"
#include "HotKey.hh"
#include <memory>

using std::auto_ptr;
using std::string;
using std::vector;

namespace openmsx {

SettingsConfig::SettingsConfig()
	: XMLElement("settings")
	, saveSettingsCommand(*this)
	, loadSettingsCommand(*this)
	, hotKey(0)
	, mustSaveSettings(false)
{
	setFileContext(auto_ptr<FileContext>(new SystemFileContext()));
	CommandController::instance().
		registerCommand(&saveSettingsCommand, "save_settings");
	CommandController::instance().
		registerCommand(&loadSettingsCommand, "load_settings");
}

SettingsConfig::~SettingsConfig()
{
	assert(!hotKey);
	if (mustSaveSettings) {
		try {
			saveSetting();
		} catch (FileException& e) {
			CliComm::instance().printWarning(
				"Auto-saving of settings failed: " + e.getMessage() );
		}
	}
	CommandController::instance().
		unregisterCommand(&loadSettingsCommand, "load_settings");
	CommandController::instance().
		unregisterCommand(&saveSettingsCommand, "save_settings");
}

SettingsConfig& SettingsConfig::instance()
{
	static SettingsConfig oneInstance;
	return oneInstance;
}

void SettingsConfig::setHotKey(HotKey* hotKey_)
{
	hotKey = hotKey_;
}

void SettingsConfig::loadSetting(FileContext& context, const string& filename)
{
	assert(hotKey);
	try {
		saveName = context.resolveCreate(filename);
		File file(context.resolve(filename));
		auto_ptr<XMLElement> doc(XMLLoader::loadXML(
			file.getLocalName(), "settings.dtd"));
		merge(*doc);
		hotKey->loadBindings(*this);
	} catch (XMLException& e) {
		CliComm::instance().printWarning(
			"Loading of settings failed: " + e.getMessage() + "\n"
			"Reverting to default settings.");
	}
}

void SettingsConfig::saveSetting(const string& filename)
{
	const string& name = filename.empty() ? saveName : filename;
	if (name.empty()) return;

	XMLElement copy(*this);
	XMLElement* copySettings = copy.findChild("settings");
	if (copySettings) {
		vector<const Setting*> allSettings;
		getSettingsManager().getAllSettings(allSettings);
		for (vector<const Setting*>::const_iterator it = allSettings.begin();
		     it != allSettings.end(); ++it) {
			const Setting& setting = **it;
			if (setting.hasDefaultValue()) {
				// don't save settings that have their default value
				XMLElement* elem = copySettings->findChildWithAttribute(
					"setting", "id", setting.getName());
				if (elem) {
					copySettings->removeChild(*elem);
				}
			}
		}
	}
	/* TODO reenable when singleton web is cleaned up
	  assert(hotKey);
	  hotKey->saveBindings(*this);
	*/
	
	File file(name, TRUNCATE);
	string data = "<!DOCTYPE settings SYSTEM 'settings.dtd'>\n" +
	              copy.dump();
	file.write((const byte*)data.c_str(), data.size());
}

void SettingsConfig::setSaveSettings(bool save)
{
	mustSaveSettings = save;
}

SettingsManager& SettingsConfig::getSettingsManager()
{
	if (!settingsManager.get()) {
		settingsManager.reset(new SettingsManager());
	}
	return *settingsManager;
}


// class SaveSettingsCommand

SettingsConfig::SaveSettingsCommand::SaveSettingsCommand(
	SettingsConfig& parent_)
	: parent(parent_)
{
}

string SettingsConfig::SaveSettingsCommand::execute(
	const vector<string>& tokens)
{
	try {
		switch (tokens.size()) {
			case 1:
				parent.saveSetting();
				break;
			case 2:
				parent.saveSetting(tokens[1]);
				break;
			default:
				throw SyntaxError();
		}
	} catch (FileException& e) {
		throw CommandException(e.getMessage());
	}
	return "";
}

string SettingsConfig::SaveSettingsCommand::help(
	const vector<string>& /*tokens*/) const
{
	return "Save the current settings.";
}

void SettingsConfig::SaveSettingsCommand::tabCompletion(
	vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		CommandController::completeFileName(tokens);
	}
}


// class LoadSettingsCommand

SettingsConfig::LoadSettingsCommand::LoadSettingsCommand(
	SettingsConfig& parent_)
	: parent(parent_)
{
}

string SettingsConfig::LoadSettingsCommand::execute(
	const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	SystemFileContext context;
	parent.loadSetting(context, tokens[1]);
	return "";
}

string SettingsConfig::LoadSettingsCommand::help(
	const vector<string>& /*tokens*/) const
{
	return "Load settings from given file.";
}

void SettingsConfig::LoadSettingsCommand::tabCompletion(
	vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		CommandController::completeFileName(tokens);
	}
}

} // namespace openmsx
