// $Id$

#include "SettingsConfig.hh"
#include "SettingsManager.hh"
#include "XMLLoader.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CliComm.hh"
#include "HotKey.hh"
#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "Command.hh"
#include <memory>

using std::auto_ptr;
using std::string;
using std::vector;

namespace openmsx {

class SaveSettingsCommand : public SimpleCommand
{
public:
	SaveSettingsCommand(CommandController& commandController,
			    SettingsConfig& settingsConfig);
	virtual string execute(const vector<string>& tokens);
	virtual string help   (const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	SettingsConfig& settingsConfig;
};

class LoadSettingsCommand : public SimpleCommand
{
public:
	LoadSettingsCommand(CommandController& commandController,
			    SettingsConfig& settingsConfig);
	virtual string execute(const vector<string>& tokens);
	virtual string help   (const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	SettingsConfig& settingsConfig;
};


SettingsConfig::SettingsConfig(GlobalCommandController& commandController_,
                               HotKey& hotKey_)
	: XMLElement("settings")
	, commandController(commandController_)
	, saveSettingsCommand(new SaveSettingsCommand(commandController, *this))
	, loadSettingsCommand(new LoadSettingsCommand(commandController, *this))
	, hotKey(hotKey_)
	, mustSaveSettings(false)
{
	setFileContext(auto_ptr<FileContext>(new SystemFileContext()));
	settingsManager.reset(new SettingsManager(commandController));
}

SettingsConfig::~SettingsConfig()
{
	if (mustSaveSettings) {
		try {
			saveSetting();
		} catch (FileException& e) {
			commandController.getCliComm().printWarning(
				"Auto-saving of settings failed: " + e.getMessage() );
		}
	}
}

void SettingsConfig::loadSetting(FileContext& context, const string& filename)
{
	try {
		saveName = context.resolveCreate(filename);
		File file(context.resolve(filename));
		auto_ptr<XMLElement> doc(XMLLoader::loadXML(
			file.getLocalName(), "settings.dtd"));
		XMLElement::operator=(*doc);
		getSettingsManager().loadSettings(*this);
		hotKey.loadBindings(*this);
	} catch (XMLException& e) {
		commandController.getCliComm().printWarning(
			"Loading of settings failed: " + e.getMessage() + "\n"
			"Reverting to default settings.");
	}
}

void SettingsConfig::saveSetting(const string& filename)
{
	const string& name = filename.empty() ? saveName : filename;
	if (name.empty()) return;

	getSettingsManager().saveSettings(*this);
	hotKey.saveBindings(*this);

	File file(name, File::TRUNCATE);
	string data = "<!DOCTYPE settings SYSTEM 'settings.dtd'>\n" + dump();
	file.write((const byte*)data.c_str(), data.size());
}

void SettingsConfig::setSaveSettings(bool save)
{
	mustSaveSettings = save;
}

SettingsManager& SettingsConfig::getSettingsManager()
{
	return *settingsManager; // ***
}


// class SaveSettingsCommand

SaveSettingsCommand::SaveSettingsCommand(
		CommandController& commandController,
		SettingsConfig& settingsConfig_)
	: SimpleCommand(commandController, "save_settings")
	, settingsConfig(settingsConfig_)
{
}

string SaveSettingsCommand::execute(const vector<string>& tokens)
{
	try {
		switch (tokens.size()) {
			case 1:
				settingsConfig.saveSetting();
				break;
			case 2:
				settingsConfig.saveSetting(tokens[1]);
				break;
			default:
				throw SyntaxError();
		}
	} catch (FileException& e) {
		throw CommandException(e.getMessage());
	}
	return "";
}

string SaveSettingsCommand::help(const vector<string>& /*tokens*/) const
{
	return "Save the current settings.";
}

void SaveSettingsCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		SystemFileContext context;
		completeFileName(tokens, context);
	}
}


// class LoadSettingsCommand

LoadSettingsCommand::LoadSettingsCommand(
		CommandController& commandController,
		SettingsConfig& settingsConfig_)
	: SimpleCommand(commandController, "load_settings")
	, settingsConfig(settingsConfig_)
{
}

string LoadSettingsCommand::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	SystemFileContext context;
	settingsConfig.loadSetting(context, tokens[1]);
	return "";
}

string LoadSettingsCommand::help(const vector<string>& /*tokens*/) const
{
	return "Load settings from given file.";
}

void LoadSettingsCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		SystemFileContext context;
		completeFileName(tokens, context);
	}
}

} // namespace openmsx
