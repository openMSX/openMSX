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
	enum ParseStatus { UNPARSED, RUN, CONTROL, EXIT };
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
	HardwareConfig& hardwareConfig;
	SettingsConfig& settingsConfig;
	CliComm& output;

	class HelpOption : public CLIOption {
	public:
		HelpOption(CommandLineParser& parent);
		virtual ~HelpOption();
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} helpOption;

	class VersionOption : public CLIOption {
	public:
		VersionOption(CommandLineParser& parent);
		virtual ~VersionOption();
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} versionOption;

	class ControlOption : public CLIOption {
	public:
		ControlOption(CommandLineParser& parent);
		virtual ~ControlOption();
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} controlOption;

	class MachineOption : public CLIOption {
	public:
		MachineOption(CommandLineParser& parent);
		virtual ~MachineOption();
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} machineOption;

	class SettingOption : public CLIOption {
	public:
		SettingOption(CommandLineParser& parent);
		virtual ~SettingOption();
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} settingOption;

	class NoMMXOption : public CLIOption {
	public:
		NoMMXOption(CommandLineParser& parent);
		virtual ~NoMMXOption();
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} noMMXOption;

	class NoMMXEXTOption : public CLIOption {
	public:
		NoMMXEXTOption(CommandLineParser& parent);
		virtual ~NoMMXEXTOption();
		virtual bool parseOption(const std::string& option,
			std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} noMMXEXTOption;

	const std::auto_ptr<MSXRomCLI> msxRomCLI;
	const std::auto_ptr<CliExtension> cliExtension;
	const std::auto_ptr<MSXCassettePlayerCLI> cassettePlayerCLI;
	const std::auto_ptr<DiskImageCLI> diskImageCLI;
	std::auto_ptr<EnumSetting<int> > machineSetting;
};

} // namespace openmsx

#endif
