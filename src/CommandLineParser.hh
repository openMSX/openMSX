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
		virtual void parseOption(const std::string &option,
		                         std::list<std::string> &cmdLine) = 0;
		virtual const std::string& optionHelp() const = 0;
	
	protected:
		const std::string getArgument(const std::string &option,
		                              std::list<std::string> &cmdLine);
};

class CLIFileType
{
	public:
		virtual void parseFileType(const std::string &filename) = 0;
		virtual const std::string& fileTypeHelp() const = 0;
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
		
		void registerOption(const std::string &str, CLIOption* cliOption);
		void registerFileType(const std::string &str, CLIFileType* cliFileType);
		void registerPostConfig(CLIPostConfig *post);
		void parse(int argc, char **argv);
		bool parseFileName(const std::string &arg,std::list<std::string> &cmdLine);
		bool parseOption(const std::string &arg,std::list<std::string> &cmdLine);

	//private: // should be private, but gcc-2.95.x complains
		struct caseltstr {
			bool operator()(const std::string s1, const std::string s2) const {
				return strcasecmp(s1.c_str(), s2.c_str()) < 0;
			}
		};
		std::map<std::string, CLIOption*> optionMap;
		std::map<std::string, CLIFileType*, caseltstr> fileTypeMap;
		std::map<std::string, CLIFileType*, caseltstr> fileClassMap;
	private:
		CommandLineParser();
		void postRegisterFileTypes();
		std::vector<CLIPostConfig*> postConfigs;
		bool haveConfig;

		class HelpOption : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp() const;
			std::string formatSet(std::set<std::string> * inputSet,unsigned columns);
			std::string formatHelptext(std::string helpText,unsigned maxlength, unsigned indent);
		} helpOption;
		
		class ConfigFile : public CLIOption, public CLIFileType {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp() const;
			virtual void parseFileType(const std::string &filename);
			virtual const std::string& fileTypeHelp() const;
		} configFile;
		friend class ConfigFile;
		
		class MachineOption : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp() const;
		} machineOption;
		friend class MachineOption;
		
		class SettingOption : public CLIOption {
		public:
			SettingOption() : parsed(false) {}
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp() const;
			bool parsed;
		} settingOption;
};

#endif
