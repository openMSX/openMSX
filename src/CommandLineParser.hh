// $Id$

#ifndef __COMMANDLINEPARSER_HH__
#define __COMMANDLINEPARSER_HH__

#include <string>
#include <list>
#include <map>
#include "openmsx.hh"
#include "config.h"
#include "MSXConfig.hh"


class CLIOption
{
	public:
		virtual void parseOption(const std::string &option,
		                     std::list<std::string> &cmdLine) = 0;
		virtual const std::string& optionHelp() = 0;
};

class CLIFileType
{
	public:
		virtual void parseFileType(const std::string &filename) = 0;
		virtual const std::string& fileTypeHelp() = 0;
};


class CommandLineParser
{
	public:
		static CommandLineParser* instance();
		
		void registerOption(const std::string &str, CLIOption* cliOption);
		void registerFileType(const std::string &str, CLIFileType* cliFileType);
		void parse(int argc, char **argv);

	private:
		CommandLineParser();

		struct caseltstr {
			bool operator()(const std::string s1, const std::string s2) const {
				return strcasecmp(s1.c_str(), s2.c_str()) < 0;
			}
		};
		std::map<std::string, CLIOption*> optionMap;
		std::map<std::string, CLIFileType*, caseltstr> fileTypeMap;

		bool haveConfig;

		class HelpOption : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp();
		} helpOption;
		
		class ConfigFile : public CLIFileType {
		public:
			virtual void parseFileType(const std::string &filename);
			virtual const std::string& fileTypeHelp();
		} configFile;
		friend class ConfigFile;
		
		class MSX1Option : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp();
		} msx1Option;
		friend class MSX1Option;
		
		class MSX2Option : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp();
		} msx2Option;
		friend class MSX2Option;
		
		class MSX2POption : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp();
		} msx2POption;
		friend class MSX2POption;
		
		class MSXTurboROption : public CLIOption {
		public:
			virtual void parseOption(const std::string &option,
				std::list<std::string> &cmdLine);
			virtual const std::string& optionHelp();
		} msxTurboROption;
		friend class MSXTurboROption;
};

#endif
