// $Id$

#include "xmlx.hh"
#include "File.hh"
#include "FileContext.hh"
#include "SettingsConfig.hh"
#include "CommandController.hh"

namespace openmsx {

SettingsConfig::SettingsConfig()
	: MSXConfig("settings")
	, saveSettingsCommand(*this)
{
	CommandController::instance().
		registerCommand(&saveSettingsCommand, "save_settings");
}

SettingsConfig::~SettingsConfig()
{
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
	loadName = context.resolve(filename);
	File file(loadName);
	XMLDocument doc(file.getLocalName());
	handleDoc(*this, doc, context);
}

void SettingsConfig::saveSetting(const string& filename)
{
	const string& saveName = filename.empty() ? loadName : filename;
	string data = dump();
	File file(saveName, TRUNCATE);
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
