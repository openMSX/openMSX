// $Id$

#ifndef __COMMANDLINEPARSER_HH__
#define __COMMANDLINEPARSER_HH__

#include <string>
#include <list>
#include <map>
#include <vector>
#include <set>
#include <memory>
#include "openmsx.hh"
#include "StringOp.hh"
#include "EnumSetting.hh"

using std::string;
using std::list;
using std::map;
using std::vector;
using std::set;
using std::auto_ptr;

namespace openmsx {

class HardwareConfig;
class SettingsConfig;
class CliCommOutput;
class MSXRomCLI;
class CliExtension;
class MSXCassettePlayerCLI;
class MSXCasCLI;
class DiskImageCLI;
class SettingsManager;

class CLIOption
{
public:
	virtual ~CLIOption() {}
	virtual bool parseOption(const string& option,
	                         list<string>& cmdLine) = 0;
	virtual const string& optionHelp() const = 0;

protected:
	const string getArgument(const string& option,
	                         list<string>& cmdLine);
};

class CLIFileType
{
public:
	virtual ~CLIFileType() {}
	virtual void parseFileType(const string& filename) = 0;
	virtual const string& fileTypeHelp() const = 0;
};

struct OptionData
{
	CLIOption* option;
	byte prio;
	byte length; // length in parameters
};

class CommandLineParser
{
public:
	enum ParseStatus { UNPARSED, RUN, CONTROL, EXIT };
	enum ControlType { IO_STD, IO_PIPE };
	static CommandLineParser& instance();
	void getControlParameters (ControlType& type, string& arguments);
	void registerOption(const string& str, CLIOption* cliOption,
		byte prio = 7, byte length = 2);
	void registerFileClass(const string& str, CLIFileType* cliFileType);
	void parse(int argc, char** argv);
	ParseStatus getParseStatus() const;
	bool wantSound() const;

private:
	CommandLineParser();
	~CommandLineParser();
	bool parseFileName(const string& arg,list<string>& cmdLine);
	bool parseOption(const string& arg,list<string>& cmdLine, byte prio);
	void postRegisterFileTypes();
	void loadMachine(const string& machine);
	void createMachineSetting();

	map<string, OptionData> optionMap;
	typedef map<string, CLIFileType*, StringOp::caseless> FileTypeMap;
	FileTypeMap fileTypeMap;
	typedef map<string, CLIFileType*, StringOp::caseless> FileClassMap;
	FileClassMap fileClassMap;

	bool haveConfig;
	bool haveSettings;
	bool issuedHelp;
	ParseStatus parseStatus;
	bool sound;

	HardwareConfig& hardwareConfig;
	SettingsConfig& settingsConfig;
	CliCommOutput& output;
	SettingsManager& settingsManager;

	class HelpOption : public CLIOption {
	public:
		HelpOption(CommandLineParser& parent);
		virtual ~HelpOption();
		virtual bool parseOption(const string& option,
			list<string>& cmdLine);
		virtual const string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} helpOption;

	class VersionOption : public CLIOption {
	public:
		VersionOption(CommandLineParser& parent);
		virtual ~VersionOption();
		virtual bool parseOption(const string& option,
			list<string>& cmdLine);
		virtual const string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} versionOption;

	class ControlOption : public CLIOption {
	public:
		ControlOption(CommandLineParser& parent);
		virtual ~ControlOption();
		virtual bool parseOption(const string& option,
			list<string>& cmdLine);
		virtual const string& optionHelp() const;
		CommandLineParser::ControlType type;
		string arguments;
	private:
		CommandLineParser& parent;
	} controlOption;

	class MachineOption : public CLIOption {
	public:
		MachineOption(CommandLineParser& parent);
		virtual ~MachineOption();
		virtual bool parseOption(const string& option,
			list<string>& cmdLine);
		virtual const string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} machineOption;

	class SettingOption : public CLIOption {
	public:
		SettingOption(CommandLineParser& parent);
		virtual ~SettingOption();
		virtual bool parseOption(const string& option,
			list<string>& cmdLine);
		virtual const string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} settingOption;
	
	class NoSoundOption : public CLIOption {
	public:
		NoSoundOption(CommandLineParser& parent);
		virtual ~NoSoundOption();
		virtual bool parseOption(const string& option,
			list<string>& cmdLine);
		virtual const string& optionHelp() const;
	private:
		CommandLineParser& parent;
	} noSoundOption;
	
	const auto_ptr<MSXRomCLI> msxRomCLI;
	const auto_ptr<CliExtension> cliExtension;
	const auto_ptr<MSXCassettePlayerCLI> cassettePlayerCLI;
	const auto_ptr<MSXCasCLI> casCLI;
	const auto_ptr<DiskImageCLI> diskImageCLI;
	auto_ptr<EnumSetting<int> > machineSetting;
};

} // namespace openmsx

#endif // __COMMANDLINEPARSER_HH__
