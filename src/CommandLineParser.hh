// $Id$

#ifndef __COMMANDLINEPARSER_HH__
#define __COMMANDLINEPARSER_HH__

#include <string>
#include <list>
#include <map>
#include <vector>
#include <set>
#include "openmsx.hh"
#include "config.h"

using std::string;
using std::list;
using std::map;
using std::vector;
using std::set;


namespace openmsx {

class HardwareConfig;
class SettingsConfig;
class CliCommOutput;
class CartridgeSlotManager;


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

class CLIPostConfig
{
public:
	virtual ~CLIPostConfig() {}
	virtual void execute() = 0;
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
	void registerOption(const string& str, CLIOption* cliOption, byte prio = 7, byte length = 2);
	void registerFileClass(const string& str, CLIFileType* cliFileType);
	void registerPostConfig(CLIPostConfig* post);
	void parse(int argc, char** argv);
	ParseStatus getParseStatus() const;

//private: // should be private, but gcc-2.95.x complains
	struct caseltstr {
		bool operator()(const string& s1, const string& s2) const {
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
		}
	};
	map<string, OptionData> optionMap;
	map<string, CLIFileType*, caseltstr> fileTypeMap;
	map<string, CLIFileType*, caseltstr> fileClassMap;

private:
	bool parseFileName(const string& arg,list<string>& cmdLine);
	bool parseOption(const string& arg,list<string>& cmdLine, byte prio);

	CommandLineParser();
	~CommandLineParser();
	void postRegisterFileTypes();
	vector<CLIPostConfig*> postConfigs;
	bool haveConfig;
	bool haveSettings;
	bool issuedHelp;
	ParseStatus parseStatus;

	HardwareConfig& hardwareConfig;
	SettingsConfig& settingsConfig;
	CliCommOutput& output;
	CartridgeSlotManager& slotManager;

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
};

} // namespace openmsx

#endif // __COMMANDLINEPARSER_HH__
