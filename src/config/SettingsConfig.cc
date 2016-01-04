#include "SettingsConfig.hh"
#include "XMLLoader.hh"
#include "LocalFileReference.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CliComm.hh"
#include "HotKey.hh"
#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "outer.hh"

using std::string;
using std::vector;

namespace openmsx {

SettingsConfig::SettingsConfig(
		GlobalCommandController& globalCommandController,
		HotKey& hotKey_)
	: commandController(globalCommandController)
	, saveSettingsCommand(commandController)
	, loadSettingsCommand(commandController)
	, settingsManager(globalCommandController)
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

SettingsConfig::SaveSettingsCommand::SaveSettingsCommand(
		CommandController& commandController_)
	: Command(commandController_, "save_settings")
{
}

void SettingsConfig::SaveSettingsCommand::execute(
	array_ref<TclObject> tokens, TclObject& /*result*/)
{
	auto& settingsConfig = OUTER(SettingsConfig, saveSettingsCommand);
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

string SettingsConfig::SaveSettingsCommand::help(const vector<string>& /*tokens*/) const
{
	return "Save the current settings.";
}

void SettingsConfig::SaveSettingsCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		completeFileName(tokens, systemFileContext());
	}
}


// class LoadSettingsCommand

SettingsConfig::LoadSettingsCommand::LoadSettingsCommand(
		CommandController& commandController_)
	: Command(commandController_, "load_settings")
{
}

void SettingsConfig::LoadSettingsCommand::execute(
	array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	auto& settingsConfig = OUTER(SettingsConfig, loadSettingsCommand);
	settingsConfig.loadSetting(systemFileContext(), tokens[1].getString());
}

string SettingsConfig::LoadSettingsCommand::help(const vector<string>& /*tokens*/) const
{
	return "Load settings from given file.";
}

void SettingsConfig::LoadSettingsCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		completeFileName(tokens, systemFileContext());
	}
}

} // namespace openmsx
