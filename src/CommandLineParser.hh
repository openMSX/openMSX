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
#include "span.hh"
#include "components.hh"
#include <string>
#include <string_view>
#include <vector>
#include <utility>

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
	enum ParseStatus { UNPARSED, RUN, CONTROL, TEST, EXIT };
	enum ParsePhase {
		PHASE_BEFORE_INIT,       // --help, --version, -bash
		PHASE_INIT,              // calls Reactor::init()
		PHASE_BEFORE_SETTINGS,   // -setting, -nommx, ...
		PHASE_LOAD_SETTINGS,     // loads settings.xml
		PHASE_BEFORE_MACHINE,    // before -machine
		PHASE_LOAD_MACHINE,      // -machine
		PHASE_DEFAULT_MACHINE,   // default machine
		PHASE_LAST,              // all the rest
	};

	explicit CommandLineParser(Reactor& reactor);
	void registerOption(const char* str, CLIOption& cliOption,
		ParsePhase phase = PHASE_LAST, unsigned length = 2);
	void registerFileType(std::string_view extensions, CLIFileType& cliFileType);
	void parse(int argc, char** argv);
	ParseStatus getParseStatus() const;

	using Scripts = std::vector<std::string>;
	const Scripts& getStartupScripts() const;

	MSXMotherBoard* getMotherBoard() const;
	GlobalCommandController& getGlobalCommandController() const;
	Interpreter& getInterpreter() const;

	/** Need to suppress renderer window on startup?
	  */
	bool isHiddenStartup() const;

private:
	struct OptionData {
		CLIOption* option;
		ParsePhase phase;
		unsigned length; // length in parameters
	};

	bool parseFileName(const std::string& arg,
	                   span<std::string>& cmdLine);
	bool parseFileNameInner(const std::string& arg, const std::string&
	                   originalPath, span<std::string>& cmdLine);
	bool parseOption(const std::string& arg,
	                 span<std::string>& cmdLine, ParsePhase phase);
	void createMachineSetting();

	std::vector<std::pair<std::string_view, OptionData>> options;
	std::vector<std::pair<std::string_view, CLIFileType*>> fileTypes;

	Reactor& reactor;

	struct HelpOption final : CLIOption {
		void parseOption(const std::string& option, span<std::string>& cmdLine) override;
		std::string_view optionHelp() const override;
	} helpOption;

	struct VersionOption final : CLIOption {
		void parseOption(const std::string& option, span<std::string>& cmdLine) override;
		std::string_view optionHelp() const override;
	} versionOption;

	struct ControlOption final : CLIOption {
		void parseOption(const std::string& option, span<std::string>& cmdLine) override;
		std::string_view optionHelp() const override;
	} controlOption;

	struct ScriptOption final : CLIOption, CLIFileType {
		void parseOption(const std::string& option, span<std::string>& cmdLine) override;
		std::string_view optionHelp() const override;
		void parseFileType(const std::string& filename,
				   span<std::string>& cmdLine) override;
		std::string_view fileTypeHelp() const override;

		CommandLineParser::Scripts scripts;
	} scriptOption;

	struct MachineOption final : CLIOption {
		void parseOption(const std::string& option, span<std::string>& cmdLine) override;
		std::string_view optionHelp() const override;
	} machineOption;

	struct SettingOption final : CLIOption {
		void parseOption(const std::string& option, span<std::string>& cmdLine) override;
		std::string_view optionHelp() const override;
	} settingOption;

	struct NoPBOOption final : CLIOption {
		void parseOption(const std::string& option, span<std::string>& cmdLine) override;
		std::string_view optionHelp() const override;
	} noPBOOption;

	struct TestConfigOption final : CLIOption {
		void parseOption(const std::string& option, span<std::string>& cmdLine) override;
		std::string_view optionHelp() const override;
	} testConfigOption;

	struct BashOption final : CLIOption {
		void parseOption(const std::string& option, span<std::string>& cmdLine) override;
		std::string_view optionHelp() const override;
	} bashOption;

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
	ParseStatus parseStatus;
	bool haveConfig;
	bool haveSettings;
};

} // namespace openmsx

#endif
