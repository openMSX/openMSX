// $Id$

#ifndef COMMANDLINEPARSER_HH
#define COMMANDLINEPARSER_HH

#include "StringOp.hh"
#include "noncopyable.hh"
#include <string>
#include <vector>
#include <list>
#include <map>
#include <memory>

namespace openmsx {

class CLIOption;
class CLIFileType;
class SettingsConfig;
class CliComm;
class HelpOption;
class VersionOption;
class ControlOption;
class ScriptOption;
class MachineOption;
class SettingOption;
class NoMMXOption;
class NoMMXEXTOption;
class TestConfigOption;
class MSXRomCLI;
class CliExtension;
class CassettePlayerCLI;
class DiskImageCLI;
class HDImageCLI;
class CDImageCLI;
class Reactor;
class MSXMotherBoard;
class MSXEventRecorderReplayerCLI;

class CommandLineParser : private noncopyable
{
public:
	enum ParseStatus { UNPARSED, RUN, CONTROL, TEST, EXIT };

	explicit CommandLineParser(Reactor& reactor);
	~CommandLineParser();
	void registerOption(const std::string& str, CLIOption& cliOption,
		unsigned prio = 8, unsigned length = 2);
	void registerFileClass(const std::string& str,
	                       CLIFileType& cliFileType);
	void parse(int argc, char** argv);
	ParseStatus getParseStatus() const;

	typedef std::vector<std::string> Scripts;
	const Scripts& getStartupScripts() const;

	Reactor& getReactor() const;
	MSXMotherBoard* getMotherBoard() const;

	/** Need to suppress renderer window on startup?
	  */
	bool isHiddenStartup() const;

private:
	struct OptionData
	{
		CLIOption* option;
		unsigned prio;
		unsigned length; // length in parameters
	};

	bool parseFileName(const std::string& arg,
	                   std::list<std::string>& cmdLine);
	bool parseOption(const std::string& arg,
	                 std::list<std::string>& cmdLine, unsigned prio);
	void registerFileTypes();
	void createMachineSetting();

	std::map<std::string, OptionData> optionMap;
	typedef std::map<std::string, CLIFileType*, StringOp::caseless> FileTypeMap;
	FileTypeMap fileTypeMap;
	typedef std::map<std::string, CLIFileType*, StringOp::caseless> FileClassMap;
	FileClassMap fileClassMap;

	bool haveConfig;
	bool haveSettings;
	ParseStatus parseStatus;

	Reactor& reactor;
	SettingsConfig& settingsConfig;
	CliComm& output;

	const std::auto_ptr<HelpOption> helpOption;
	const std::auto_ptr<VersionOption> versionOption;
	const std::auto_ptr<ControlOption> controlOption;
	const std::auto_ptr<ScriptOption> scriptOption;
	const std::auto_ptr<MachineOption> machineOption;
	const std::auto_ptr<SettingOption> settingOption;
	const std::auto_ptr<NoMMXOption> noMMXOption;
	const std::auto_ptr<NoMMXEXTOption> noMMXEXTOption;
	const std::auto_ptr<TestConfigOption> testConfigOption;

	const std::auto_ptr<MSXRomCLI> msxRomCLI;
	const std::auto_ptr<CliExtension> cliExtension;
	const std::auto_ptr<CassettePlayerCLI> cassettePlayerCLI;
	const std::auto_ptr<DiskImageCLI> diskImageCLI;
	const std::auto_ptr<HDImageCLI> hdImageCLI;
	const std::auto_ptr<CDImageCLI> cdImageCLI;
	const std::auto_ptr<MSXEventRecorderReplayerCLI> 
					eventRecorderReplayerCLI;
	
	bool hiddenStartup;

	friend class ControlOption;
	friend class HelpOption;
	friend class VersionOption;
	friend class MachineOption;
	friend class SettingOption;
	friend class TestConfigOption;
};

} // namespace openmsx

#endif
