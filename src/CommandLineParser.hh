// $Id$

#ifndef COMMANDLINEPARSER_HH
#define COMMANDLINEPARSER_HH

#include "StringOp.hh"
#include "openmsx.hh"
#include <string>
#include <vector>
#include <list>
#include <map>
#include <memory>

namespace openmsx {

class SettingsConfig;
class CliComm;
class MSXRomCLI;
class CliExtension;
class MSXCassettePlayerCLI;
class DiskImageCLI;
class Reactor;
class MSXMotherBoard;

class CLIOption
{
public:
	virtual ~CLIOption() {}
	virtual bool parseOption(const std::string& option,
	                         std::list<std::string>& cmdLine) = 0;
	virtual const std::string& optionHelp() const = 0;

protected:
	std::string getArgument(const std::string& option,
	                              std::list<std::string>& cmdLine) const;
	std::string peekArgument(const std::list<std::string>& cmdLine) const;
};

class CLIFileType
{
public:
	virtual ~CLIFileType() {}
	virtual void parseFileType(const std::string& filename,
	                           std::list<std::string>& cmdLine) = 0;
	virtual const std::string& fileTypeHelp() const = 0;
};

class CommandLineParser
{
public:
	enum ParseStatus { UNPARSED, RUN, CONTROL, TEST, EXIT };
	enum ControlType { IO_STD, IO_PIPE };

	explicit CommandLineParser(Reactor& reactor);
	~CommandLineParser();
	void registerOption(const std::string& str, CLIOption* cliOption,
		byte prio = 8, byte length = 2);
	void registerFileClass(const std::string& str,
	                       CLIFileType* cliFileType);
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
		byte prio;
		byte length; // length in parameters
	};

	bool parseFileName(const std::string& arg,
	                   std::list<std::string>& cmdLine);
	bool parseOption(const std::string& arg,
	                 std::list<std::string>& cmdLine, byte prio);
	void registerFileTypes();
	void loadMachine(const std::string& machine);
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

	class HelpOption : public CLIOption {
	public:
		HelpOption(CommandLineParser& parser);
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parser;
	} helpOption;

	class VersionOption : public CLIOption {
	public:
		VersionOption(CommandLineParser& parser);
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parser;
	} versionOption;

	class ControlOption : public CLIOption {
	public:
		ControlOption(CommandLineParser& parser);
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parser;
	} controlOption;

	class ScriptOption : public CLIOption {
	public:
		const Scripts& getScripts() const;
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		Scripts scripts;
	} scriptOption;

	class MachineOption : public CLIOption {
	public:
		MachineOption(CommandLineParser& parser);
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parser;
	} machineOption;

	class SettingOption : public CLIOption {
	public:
		SettingOption(CommandLineParser& parser);
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parser;
	} settingOption;

	class NoMMXOption : public CLIOption {
	public:
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	} noMMXOption;

	class NoMMXEXTOption : public CLIOption {
	public:
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	} noMMXEXTOption;

	class TestConfigOption : public CLIOption {
	public:
		TestConfigOption(CommandLineParser& parser);
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parser;
	} testConfigOption;

	const std::auto_ptr<MSXRomCLI> msxRomCLI;
	const std::auto_ptr<CliExtension> cliExtension;
	const std::auto_ptr<MSXCassettePlayerCLI> cassettePlayerCLI;
	const std::auto_ptr<DiskImageCLI> diskImageCLI;
	
	bool hiddenStartup;
};

} // namespace openmsx

#endif
