#include "SettingsConfig.hh"
#include "XMLLoader.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "CliComm.hh"
#include "HotKey.hh"
#include "CommandException.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "outer.hh"

using std::string;
using std::string_view;
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
				"Auto-saving of settings failed: ", e.getMessage());
		}
	}
}

void SettingsConfig::loadSetting(const FileContext& context, string_view filename)
{
	string resolved = context.resolve(filename);
	xmlElement = XMLLoader::load(resolved, "settings.dtd");
	getSettingsManager().loadSettings(xmlElement);
	hotKey.loadBindings(xmlElement);

	// only set saveName after file was successfully parsed
	saveName = resolved;
}

void SettingsConfig::setSaveFilename(const FileContext& context, string_view filename)
{
	saveName = context.resolveCreate(filename);
}

void SettingsConfig::saveSetting(string filename)
{
	if (filename.empty()) filename = saveName;
	if (filename.empty()) return;

	// Normally the following isn't needed. Only when there was no
	// settings.xml in either the user or the system directory (so an
	// incomplete openMSX installation!!) the top level element will have
	// an empty name. And we shouldn't write an invalid xml file.
	xmlElement.setName("settings");

	// settings are kept up-to-date
	hotKey.saveBindings(xmlElement);

	File file(std::move(filename), File::TRUNCATE);
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
	span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, Between{1, 2}, Prefix{1}, "?filename?");
	auto& settingsConfig = OUTER(SettingsConfig, saveSettingsCommand);
	try {
		switch (tokens.size()) {
		case 1:
			settingsConfig.saveSetting();
			break;
		case 2:
			settingsConfig.saveSetting(FileOperations::expandTilde(
				string(tokens[1].getString())));
			break;
		}
	} catch (FileException& e) {
		throw CommandException(std::move(e).getMessage());
	}
}

string SettingsConfig::SaveSettingsCommand::help(span<const TclObject> /*tokens*/) const
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
	span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 2, "filename");
	auto& settingsConfig = OUTER(SettingsConfig, loadSettingsCommand);
	settingsConfig.loadSetting(systemFileContext(), tokens[1].getString());
}

string SettingsConfig::LoadSettingsCommand::help(span<const TclObject> /*tokens*/) const
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
