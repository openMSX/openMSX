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

class MSXConfig;


class CLIOption
{
	public:
		virtual void parseOption(const string &option,
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


class CommandLineParser
{
	public:
		static CommandLineParser* instance();
		
		void registerOption(const string &str, CLIOption* cliOption);
		void registerFileType(const string &str, CLIFileType* cliFileType);
		void registerPostConfig(CLIPostConfig *post);
		void parse(int argc, char **argv);
		bool parseFileName(const string &arg,list<string> &cmdLine);
		bool parseOption(const string &arg,list<string> &cmdLine);

	//private: // should be private, but gcc-2.95.x complains
		struct caseltstr {
			bool operator()(const string s1, const string s2) const {
				return strcasecmp(s1.c_str(), s2.c_str()) < 0;
			}
		};
		map<string, CLIOption*> optionMap;
		map<string, CLIFileType*, caseltstr> fileTypeMap;
		map<string, CLIFileType*, caseltstr> fileClassMap;
	private:
		CommandLineParser();
		void postRegisterFileTypes();
		vector<CLIPostConfig*> postConfigs;
		bool haveConfig;

		class HelpOption : public CLIOption {
		public:
			virtual void parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
			string formatSet(set<string> * inputSet,unsigned columns);
			string formatHelptext(string helpText,unsigned maxlength, unsigned indent);
		} helpOption;
		
		class ConfigFile : public CLIOption, public CLIFileType {
		public:
			virtual void parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
			virtual void parseFileType(const string &filename);
			virtual const string& fileTypeHelp() const;
		} configFile;
		friend class ConfigFile;
		
		class MachineOption : public CLIOption {
		public:
			virtual void parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
		} machineOption;
		friend class MachineOption;
		
		class SettingOption : public CLIOption {
		public:
			SettingOption() : parsed(false) {}
			virtual void parseOption(const string &option,
				list<string> &cmdLine);
			virtual const string& optionHelp() const;
			bool parsed;
		} settingOption;
};

#endif
