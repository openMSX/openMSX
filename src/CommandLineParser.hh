// $Id$

#ifndef COMMANDLINEPARSER_HH
#define COMMANDLINEPARSER_HH

#include <string>
#include <list>
#include <map>
#include <memory>
#include "openmsx.hh"
#include "StringOp.hh"

namespace openmsx {

class HardwareConfig;
class SettingsConfig;
class CliComm;
class MSXRomCLI;
class CliExtension;
class MSXCassettePlayerCLI;
class DiskImageCLI;
class MSXMotherBoard;
template <typename T> class EnumSetting;
class Setting;

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

	/**
	 * Suppress renderer window on startup.
	 * TODO: Find a better place for this; now it has to be public because
	 *       it's used in static RendererFactory::createRendererSetting.
	 */
	static bool hiddenStartup;

	CommandLineParser(MSXMotherBoard& motherBoard);
	~CommandLineParser();
	void registerOption(const std::string& str, CLIOption* cliOption,
		byte prio = 7, byte length = 2);
	void registerFileClass(const std::string& str,
	                       CLIFileType* cliFileType);
	void parse(int argc, char** argv);
	ParseStatus getParseStatus() const;

	MSXMotherBoard& getMotherBoard() const;
	HardwareConfig& getHardwareConfig() const;

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
	void postRegisterFileTypes();
	void loadMachine(const std::string& machine);
	void createMachineSetting();

	std::map<std::string, OptionData> optionMap;
	typedef std::map<std::string, CLIFileType*, StringOp::caseless> FileTypeMap;
	FileTypeMap fileTypeMap;
	typedef std::map<std::string, CLIFileType*, StringOp::caseless> FileClassMap;
	FileClassMap fileClassMap;

	bool haveConfig;
	bool haveSettings;
	bool issuedHelp;
	ParseStatus parseStatus;

	MSXMotherBoard& motherBoard;
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
	std::auto_ptr<EnumSetting<int> > machineSetting;
};

} // namespace openmsx

#endif
