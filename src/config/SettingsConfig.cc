#include "SettingsConfig.hh"
#include "SettingsManager.hh"
#include "XMLLoader.hh"
#include "LocalFileReference.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CliComm.hh"
#include "HotKey.hh"
#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "Command.hh"
#include "TclObject.hh"
#include "memory.hh"

using std::string;
using std::vector;

namespace openmsx {

class SaveSettingsCommand final : public Command
{
public:
	SaveSettingsCommand(CommandController& commandController,
	                    SettingsConfig& settingsConfig);
	void execute(array_ref<TclObject> tokens, TclObject& result) override;
	string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	SettingsConfig& settingsConfig;
};

class LoadSettingsCommand final : public Command
{
public:
	LoadSettingsCommand(CommandController& commandController,
	                    SettingsConfig& settingsConfig);
	void execute(array_ref<TclObject> tokens, TclObject& result) override;
	string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	SettingsConfig& settingsConfig;
};


SettingsConfig::SettingsConfig(
		GlobalCommandController& globalCommandController,
		HotKey& hotKey_)
	: commandController(globalCommandController)
	, saveSettingsCommand(make_unique<SaveSettingsCommand>(
		commandController, *this))
	, loadSettingsCommand(make_unique<LoadSettingsCommand>(
		commandController, *this))
	, settingsManager(make_unique<SettingsManager>(
		globalCommandController))
	, hotKey(hotKey_)
	, mustSaveSettings(false)
{
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

void SettingsConfig::loadSetting(const FileContext& context, string_ref filename)
{
	LocalFileReference file(context.resolve(filename));
	xmlElement = XMLLoader::load(file.getFilename(), "settings.dtd");
	getSettingsManager().loadSettings(xmlElement);
	hotKey.loadBindings(xmlElement);

	// only set saveName after file was successfully parsed
	setSaveFilename(context, filename);
}

void SettingsConfig::setSaveFilename(const FileContext& context, string_ref filename)
{
	saveName = context.resolveCreate(filename);
}

void SettingsConfig::saveSetting(string_ref filename)
{
	string_ref name = filename.empty() ? saveName : filename;
	if (name.empty()) return;

	// Normally the following isn't needed. Only when there was no
	// settings.xml in either the user or the system directory (so an
	// incomplete openMSX installation!!) the top level element will have
	// an empty name. And we shouldn't write an invalid xml file.
	xmlElement.setName("settings");

	// settings are kept up-to-date
	hotKey.saveBindings(xmlElement);

	File file(name, File::TRUNCATE);
	string data = "<!DOCTYPE settings SYSTEM 'settings.dtd'>\n" +
	              xmlElement.dump();
	file.write(data.data(), data.size());
}


// class SaveSettingsCommand

SaveSettingsCommand::SaveSettingsCommand(
		CommandController& commandController,
		SettingsConfig& settingsConfig_)
	: Command(commandController, "save_settings")
	, settingsConfig(settingsConfig_)
{
}

void SaveSettingsCommand::execute(array_ref<TclObject> tokens, TclObject& /*result*/)
{
	try {
		switch (tokens.size()) {
		case 1:
			settingsConfig.saveSetting();
			break;
		case 2:
			settingsConfig.saveSetting(tokens[1].getString());
			break;
		default:
			throw SyntaxError();
		}
	} catch (FileException& e) {
		throw CommandException(e.getMessage());
	}
}

string SaveSettingsCommand::help(const vector<string>& /*tokens*/) const
{
	return "Save the current settings.";
}

void SaveSettingsCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		completeFileName(tokens, SystemFileContext());
	}
}


// class LoadSettingsCommand

LoadSettingsCommand::LoadSettingsCommand(
		CommandController& commandController,
		SettingsConfig& settingsConfig_)
	: Command(commandController, "load_settings")
	, settingsConfig(settingsConfig_)
{
}

void LoadSettingsCommand::execute(array_ref<TclObject> tokens,
                                  TclObject& /*result*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	settingsConfig.loadSetting(SystemFileContext(), tokens[1].getString());
}

string LoadSettingsCommand::help(const vector<string>& /*tokens*/) const
{
	return "Load settings from given file.";
}

void LoadSettingsCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		completeFileName(tokens, SystemFileContext());
	}
}

} // namespace openmsx
