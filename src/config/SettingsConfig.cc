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
#include "XMLOutputStream.hh"
#include "outer.hh"

using std::string;

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

void SettingsConfig::loadSetting(const FileContext& context, std::string_view filename)
{
	string resolved = context.resolve(filename);
	xmlElement = XMLLoader::load(resolved, "settings.dtd");
	getSettingsManager().loadSettings(xmlElement);
	hotKey.loadBindings(xmlElement);

	// only set saveName after file was successfully parsed
	saveName = resolved;
}

void SettingsConfig::setSaveFilename(const FileContext& context, std::string_view filename)
{
	saveName = context.resolveCreate(filename);
}

void SettingsConfig::saveSetting(std::string filename)
{
	if (filename.empty()) filename = saveName;
	if (filename.empty()) return;

	struct SettingsWriter {
		SettingsWriter(std::string filename)
			: file(filename, File::TRUNCATE)
		{
			std::string_view header =
				"<!DOCTYPE settings SYSTEM 'settings.dtd'>\n";
			write(header.data(), header.size());
		}

		void write(const char* buf, size_t len) {
			file.write(buf, len);
		}
		void write1(char c) {
			file.write(&c, 1);
		}
		void check(bool condition) const {
			assert(condition);
		}

	private:
		File file;
	};

	SettingsWriter writer(filename);
	XMLOutputStream xml(writer);
	xml.begin("settings");
	{
		xml.begin("settings");
		for (const auto* s : xmlElement.getChild("settings").getChildren("setting")) {
			xml.begin("setting");
			xml.attribute("id", s->getAttributeValue("id"));
			xml.data(s->getData());
			xml.end("setting");
		}
		xml.end("settings");
	}
	{
		hotKey.saveBindings(xml);
	}
	xml.end("settings");
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

void SettingsConfig::SaveSettingsCommand::tabCompletion(std::vector<string>& tokens) const
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

void SettingsConfig::LoadSettingsCommand::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		completeFileName(tokens, systemFileContext());
	}
}

} // namespace openmsx
