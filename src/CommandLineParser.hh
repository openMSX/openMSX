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
#include "array_ref.hh"
#include "string_ref.hh"
#include "noncopyable.hh"
#include "components.hh"
#include <string>
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

class CommandLineParser : private noncopyable
{
public:
	enum ParseStatus { UNPARSED, RUN, CONTROL, TEST, EXIT };
	enum ParsePhase {
		PHASE_BEFORE_INIT,       // --help, --version, -bash
		PHASE_INIT,              // calls Reactor::init()
		PHASE_BEFORE_SETTINGS,   // -setting, -nommx, ...
		PHASE_LOAD_SETTINGS,     // loads settings.xml
		PHASE_BEFORE_MACHINE,    // -machine
		PHASE_LOAD_MACHINE,      // loads machine hardwareconfig.xml
		PHASE_LAST,              // all the rest
	};

	explicit CommandLineParser(Reactor& reactor);
	~CommandLineParser();
	void registerOption(const char* str, CLIOption& cliOption,
		ParsePhase phase = PHASE_LAST, unsigned length = 2);
	void registerFileType(string_ref extensions, CLIFileType& cliFileType);
	void parse(int argc, char** argv);
	ParseStatus getParseStatus() const;

	typedef std::vector<std::string> Scripts;
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
	                   array_ref<std::string>& cmdLine);
	bool parseFileNameInner(const std::string& arg, const std::string&
	                   originalPath, array_ref<std::string>& cmdLine);
	bool parseOption(const std::string& arg,
	                 array_ref<std::string>& cmdLine, ParsePhase phase);
	void createMachineSetting();

	std::vector<std::pair<string_ref, OptionData>> options;
	std::vector<std::pair<string_ref, CLIFileType*>> fileTypes;

	Reactor& reactor;

	class HelpOption final : public CLIOption {
	public:
		explicit HelpOption(CommandLineParser& parser);
		void parseOption(const std::string& option, array_ref<std::string>& cmdLine) override;
		string_ref optionHelp() const override;
	private:
		CommandLineParser& parser;
	} helpOption;

	class VersionOption final : public CLIOption {
	public:
		explicit VersionOption(CommandLineParser& parser);
		void parseOption(const std::string& option, array_ref<std::string>& cmdLine) override;
		string_ref optionHelp() const override;
	private:
		CommandLineParser& parser;
	} versionOption;

	class ControlOption final : public CLIOption {
	public:
		explicit ControlOption(CommandLineParser& parser);
		void parseOption(const std::string& option, array_ref<std::string>& cmdLine) override;
		string_ref optionHelp() const override;
	private:
		CommandLineParser& parser;
	} controlOption;

	class ScriptOption final : public CLIOption, public CLIFileType {
	public:
		const CommandLineParser::Scripts& getScripts() const;
		void parseOption(const std::string& option, array_ref<std::string>& cmdLine) override;
		string_ref optionHelp() const override;
		void parseFileType(const std::string& filename,
				   array_ref<std::string>& cmdLine) override;
		string_ref fileTypeHelp() const override;
	private:
		CommandLineParser::Scripts scripts;
	} scriptOption;

	class MachineOption final : public CLIOption {
	public:
		explicit MachineOption(CommandLineParser& parser);
		void parseOption(const std::string& option, array_ref<std::string>& cmdLine) override;
		string_ref optionHelp() const override;
	private:
		CommandLineParser& parser;
	} machineOption;

	class SettingOption final : public CLIOption {
	public:
		explicit SettingOption(CommandLineParser& parser);
		void parseOption(const std::string& option, array_ref<std::string>& cmdLine) override;
		string_ref optionHelp() const override;
	private:
		CommandLineParser& parser;
	} settingOption;

	class NoPBOOption final : public CLIOption {
	public:
		void parseOption(const std::string& option, array_ref<std::string>& cmdLine) override;
		string_ref optionHelp() const override;
	} noPBOOption;

	class TestConfigOption final : public CLIOption {
	public:
		explicit TestConfigOption(CommandLineParser& parser);
		void parseOption(const std::string& option, array_ref<std::string>& cmdLine) override;
		string_ref optionHelp() const override;
	private:
		CommandLineParser& parser;
	} testConfigOption;

	class BashOption final : public CLIOption {
	public:
		explicit BashOption(CommandLineParser& parser);
		void parseOption(const std::string& option, array_ref<std::string>& cmdLine) override;
		string_ref optionHelp() const override;
	private:
		CommandLineParser& parser;
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
