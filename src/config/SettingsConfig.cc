// $Id$

#include "xmlx.hh"
#include "File.hh"
#include "FileContext.hh"
#include "SettingsConfig.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"


namespace openmsx {

SettingsConfig::SettingsConfig()
	: MSXConfig("settings")
	, autoSaveSetting(0)
	, saveSettingsCommand(*this)
{
	CommandController::instance().
		registerCommand(&saveSettingsCommand, "save_settings");
}

SettingsConfig::~SettingsConfig()
{
	if (autoSaveSetting && autoSaveSetting->getValue()) {
		try {
			saveSetting();
		} catch (FileException& e) {
			// we've tried, can't help if it fails
		}
	}
	CommandController::instance().
		unregisterCommand(&saveSettingsCommand, "save_settings");
}

SettingsConfig& SettingsConfig::instance()
{
	static SettingsConfig oneInstance;
	return oneInstance;
}

void SettingsConfig::loadSetting(FileContext& context, const string& filename)
{
	saveName = context.resolveCreate(filename);
	File file(context.resolve(filename));
	XMLDocument doc(file.getLocalName(), "settings.dtd");
	SystemFileContext systemContext;
	handleDoc(*this, doc, systemContext);

	// This setting must be retrieved earlier than the destructor.
	autoSaveSetting = &GlobalSettings::instance().getAutoSaveSetting();
}

void SettingsConfig::saveSetting(const string& filename)
{
	const string& name = filename.empty() ? saveName : filename;
	File file(name, TRUNCATE);
	string data = "<!DOCTYPE settings SYSTEM 'settings.dtd'>\n" +
	              dump();
	file.write((const byte*)data.c_str(), data.size());
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

} // namespace openmsx
