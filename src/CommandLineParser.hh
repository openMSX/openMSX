// $Id$

#ifndef __COMMANDLINEPARSER_HH__
#define __COMMANDLINEPARSER_HH__

#include <string>
#include <list>
#include <map>
#include <vector>
#include "openmsx.hh"
#include "config.h"
#include "MSXConfig.hh"


class CLIOption
{
	public:
		virtual void parseOption(const std::string &option,
		                         std::list<std::string> &cmdLine) = 0;
		virtual const std::string& optionHelp() const = 0;
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

	//private: // should be private, but gcc-2.95.x complains
		struct caseltstr {
			bool operator()(const std::string s1, const std::string s2) const {
				return strcasecmp(s1.c_str(), s2.c_str()) < 0;
			}
		};
		std::map<std::string, CLIOption*> optionMap;
		std::map<std::string, CLIFileType*, caseltstr> fileTypeMap;
		
	private:
		CommandLineParser();

		std::vector<CLIPostConfig*> postConfigs;
		bool haveConfig;

		class HelpOption : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp() const;
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
		
		class MSX1Option : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp() const;
		} msx1Option;
		friend class MSX1Option;
		
		class MSX2Option : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp() const;
		} msx2Option;
		friend class MSX2Option;
		
		class MSX2POption : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp() const;
		} msx2POption;
		friend class MSX2POption;
		
		class MSXTurboROption : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp() const;
		} msxTurboROption;
		friend class MSXTurboROption;
};

#endif
