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

class MSXConfig;


class CLIOption
{
	public:
		virtual bool parseOption(const string &option,
		                         list<string> &cmdLine) = 0;
		virtual const string& optionHelp() const = 0;
	
	protected:
		const string getArgument(const string &option,
		                         list<string> &cmdLine);
};

class CLIFileType
{
	public:
		virtual void parseFileType(const string &filename) = 0;
		virtual const string& fileTypeHelp() const = 0;
};

class CLIPostConfig
{
	public:
		virtual void execute(MSXConfig *config) = 0;
};

struct OptionData
{
	CLIOption * option;
	byte prio;
	byte length; // length in parameters
};

class CommandLineParser
{
	public:
		enum ParseStatus { OK, EXIT };
		
		static CommandLineParser* instance();
		
		void registerOption(const string &str, CLIOption* cliOption, byte prio = 7, byte length = 2);
		void registerFileClass(const string &str, CLIFileType* cliFileType);
		void registerPostConfig(CLIPostConfig *post);
		ParseStatus parse(int argc, char **argv);

	//private: // should be private, but gcc-2.95.x complains
		struct caseltstr {
			bool operator()(const string s1, const string s2) const {
				return strcasecmp(s1.c_str(), s2.c_str()) < 0;
			}
		};
		map<string, OptionData> optionMap;
		map<string, CLIFileType*, caseltstr> fileTypeMap;
		map<string, CLIFileType*, caseltstr> fileClassMap;
	
	private:
		bool parseFileName(const string &arg,list<string> &cmdLine);
		bool parseOption(const string &arg,list<string> &cmdLine, byte prio);

		CommandLineParser();
		void postRegisterFileTypes();
		vector<CLIPostConfig*> postConfigs;
		bool haveConfig;
		bool haveSettings;
		bool issuedHelp;
		ParseStatus parseStatus;

		class HelpOption : public CLIOption {
		public:
			virtual bool parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
		} helpOption;
		friend class HelpOption;
	
		class VersionOption : public CLIOption {
		public:
			virtual bool parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
		} versionOption;
		friend class VersionOption;
	
		class ControlOption : public CLIOption {
		public:
			virtual bool parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
		} controlOption;
		friend class ConfigOption;
			
		class ConfigFile : public CLIOption, public CLIFileType {
		public:
			virtual bool parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
		private:
			virtual void parseFileType(const string &filename);
			virtual const string& fileTypeHelp() const;
		} configFile;
		friend class ConfigFile;
		
		class MachineOption : public CLIOption {
		public:
			virtual bool parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
		} machineOption;
		friend class MachineOption;
		
		class SettingOption : public CLIOption {
		public:
			virtual bool parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
		} settingOption;
		friend class SettingOption;
};

} // namespace openmsx

#endif
