#ifndef COMMANDLINEPARSER_HH
#define COMMANDLINEPARSER_HH

#include "CLIOption.hh"
#include "MSXRomCLI.hh"
#include "CliExtension.hh"
#include "ReplayCLI.hh"
#include "SaveStateCLI.hh"
#include "CassettePlayerCLI.hh"
#include "DiskImageCLI.hh"
#include "HDImageCLI.hh"
#include "CDImageCLI.hh"
#include "InfoTopic.hh"

#include "components.hh"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if COMPONENT_LASERDISC
#include "LaserdiscPlayerCLI.hh"
#endif

namespace openmsx {

class Reactor;
class MSXMotherBoard;
class GlobalCommandController;
class Interpreter;

class CommandLineParser
{
public:
	enum ParseStatus : uint8_t { UNPARSED, RUN, CONTROL, TEST, EXIT };
	enum ParsePhase : uint8_t {
		PHASE_BEFORE_INIT,       // --help, --version, -bash
		PHASE_INIT,              // calls Reactor::init()
		PHASE_BEFORE_SETTINGS,   // -setting, ...
		PHASE_LOAD_SETTINGS,     // loads settings.xml
		PHASE_BEFORE_MACHINE,    // before -machine
		PHASE_LOAD_MACHINE,      // -machine
		PHASE_DEFAULT_MACHINE,   // default machine
		PHASE_LAST,              // all the rest
	};

	explicit CommandLineParser(Reactor& reactor);
	void registerOption(std::string_view str, CLIOption& cliOption,
		ParsePhase phase = PHASE_LAST, unsigned length = 2);
	void registerFileType(std::span<const std::string_view> extensions,
	                      CLIFileType& cliFileType);
	void parse(std::span<char*> argv);
	[[nodiscard]] ParseStatus getParseStatus() const;

	[[nodiscard]] const auto& getStartupScripts() const {
		return scriptOption.scripts;
	}
	[[nodiscard]] const auto& getStartupCommands() const {
		return commandOption.commands;
	}

	[[nodiscard]] MSXMotherBoard* getMotherBoard() const;
	[[nodiscard]] GlobalCommandController& getGlobalCommandController() const;
	[[nodiscard]] Interpreter& getInterpreter() const;

private:
	struct OptionData {
		std::string_view name;
		CLIOption* option;
		ParsePhase phase;
		unsigned length; // length in parameters
	};
	struct FileTypeData {
		std::string_view extension;
		CLIFileType* fileType;
	};

	[[nodiscard]] bool parseFileName(const std::string& arg,
	                   std::span<std::string>& cmdLine);
	[[nodiscard]] CLIFileType* getFileTypeHandlerForFileName(std::string_view filename) const;
	[[nodiscard]] bool parseOption(const std::string& arg,
	                 std::span<std::string>& cmdLine, ParsePhase phase);
	void createMachineSetting();

private:
	std::vector<OptionData> options;
	std::vector<FileTypeData> fileTypes;

	Reactor& reactor;

	struct HelpOption final : CLIOption {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} helpOption;

	struct VersionOption final : CLIOption {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} versionOption;

	struct ControlOption final : CLIOption {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} controlOption;

	struct ScriptOption final : CLIOption, CLIFileType {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
		void parseFileType(const std::string& filename,
				   std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view fileTypeCategoryName() const override;
		[[nodiscard]] std::string_view fileTypeHelp() const override;

		std::vector<std::string> scripts;
	} scriptOption;

	struct CommandOption final : CLIOption {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;

		std::vector<std::string> commands;
	} commandOption;

	struct MachineOption final : CLIOption {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} machineOption;

	struct SetupOption final : CLIOption {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} setupOption;

	struct SettingOption final : CLIOption {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} settingOption;

	struct TestConfigOption final : CLIOption {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} testConfigOption;

	struct BashOption final : CLIOption {
		void parseOption(const std::string& option, std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} bashOption;

	struct FileTypeCategoryInfoTopic final : InfoTopic {
		FileTypeCategoryInfoTopic(InfoCommand& openMSXInfoCommand, const CommandLineParser& parser);
		void execute(std::span<const TclObject> tokens, TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	private:
		const CommandLineParser& parser;
	};
	std::optional<FileTypeCategoryInfoTopic> fileTypeCategoryInfo;

	MSXRomCLI msxRomCLI;
	CliExtension cliExtension;
	ReplayCLI replayCLI;
	SaveStateCLI saveStateCLI;
	CassettePlayerCLI cassettePlayerCLI;
#if COMPONENT_LASERDISC
	LaserdiscPlayerCLI laserdiscPlayerCLI;
#endif
	DiskImageCLI diskImageCLI;
	HDImageCLI hdImageCLI;
	CDImageCLI cdImageCLI;
	ParseStatus parseStatus = UNPARSED;
	bool haveConfig = false;
	bool haveSettings = false;
};

} // namespace openmsx

#endif
