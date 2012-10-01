// $Id$

#ifndef COMMANDLINEPARSER_HH
#define COMMANDLINEPARSER_HH

#include "StringOp.hh"
#include "noncopyable.hh"
#include "components.hh"
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <memory>

namespace openmsx {

class CLIOption;
class CLIFileType;
class HelpOption;
class VersionOption;
class ControlOption;
class ScriptOption;
class MachineOption;
class SettingOption;
class NoMMXOption;
class NoSSEOption;
class NoSSE2Option;
class NoPBOOption;
class TestConfigOption;
class BashOption;
class MSXRomCLI;
class CliExtension;
class ReplayCLI;
class SaveStateCLI;
class CassettePlayerCLI;
class LaserdiscPlayerCLI;
class DiskImageCLI;
class HDImageCLI;
class CDImageCLI;
class Reactor;
class MSXMotherBoard;
class GlobalCommandController;

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
		PHASE_EXT,               // -ext
		PHASE_LAST,              // all the rest
	};

	explicit CommandLineParser(Reactor& reactor);
	~CommandLineParser();
	void registerOption(const char* str, CLIOption& cliOption,
		ParsePhase phase = PHASE_LAST, unsigned length = 2);
	void registerFileClass(const char* str, CLIFileType& cliFileType);
	void parse(int argc, char** argv);
	ParseStatus getParseStatus() const;

	typedef std::vector<std::string> Scripts;
	const Scripts& getStartupScripts() const;

	MSXMotherBoard* getMotherBoard() const;
	GlobalCommandController& getGlobalCommandController() const;

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
	                   std::deque<std::string>& cmdLine);
	bool parseFileNameInner(const std::string& arg, const std::string&
	                   originalPath, std::deque<std::string>& cmdLine);
	bool parseOption(const std::string& arg,
	                 std::deque<std::string>& cmdLine, ParsePhase phase);
	void registerFileTypes();
	void createMachineSetting();

	std::map<std::string, OptionData> optionMap;
	typedef std::map<std::string, CLIFileType*, StringOp::caseless> FileTypeMap;
	FileTypeMap fileTypeMap;
	typedef std::map<std::string, CLIFileType*, StringOp::caseless> FileClassMap;
	FileClassMap fileClassMap;

	Reactor& reactor;

	const std::auto_ptr<HelpOption> helpOption;
	const std::auto_ptr<VersionOption> versionOption;
	const std::auto_ptr<ControlOption> controlOption;
	const std::auto_ptr<ScriptOption> scriptOption;
	const std::auto_ptr<MachineOption> machineOption;
	const std::auto_ptr<SettingOption> settingOption;
	const std::auto_ptr<NoMMXOption> noMMXOption;
	const std::auto_ptr<NoSSEOption> noSSEOption;
	const std::auto_ptr<NoSSE2Option> noSSE2Option;
	const std::auto_ptr<NoPBOOption> noPBOOption;
	const std::auto_ptr<TestConfigOption> testConfigOption;
	const std::auto_ptr<BashOption> bashOption;

	const std::auto_ptr<MSXRomCLI> msxRomCLI;
	const std::auto_ptr<CliExtension> cliExtension;
	const std::auto_ptr<ReplayCLI> replayCLI;
	const std::auto_ptr<SaveStateCLI> saveStateCLI;
	const std::auto_ptr<CassettePlayerCLI> cassettePlayerCLI;
#if COMPONENT_LASERDISC
	const std::auto_ptr<LaserdiscPlayerCLI> laserdiscPlayerCLI;
#endif
	const std::auto_ptr<DiskImageCLI> diskImageCLI;
	const std::auto_ptr<HDImageCLI> hdImageCLI;
	const std::auto_ptr<CDImageCLI> cdImageCLI;
	ParseStatus parseStatus;
	bool haveConfig;
	bool haveSettings;
	bool hiddenStartup;

	friend class ControlOption;
	friend class HelpOption;
	friend class VersionOption;
	friend class MachineOption;
	friend class SettingOption;
	friend class TestConfigOption;
	friend class BashOption;
};

} // namespace openmsx

#endif
