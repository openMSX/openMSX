// $Id$

#include "CommandLineParser.hh"
#include "libxmlx/xmlx.hh"
#include "MSXConfig.hh"
#include "CartridgeSlotManager.hh"

#include <string>
#include <sstream>
#include <cstdio>


CommandLineParser* CommandLineParser::instance()
{
	static CommandLineParser oneInstance;
	return &oneInstance;
}

CommandLineParser::CommandLineParser()
{
	haveConfig = false;

	registerOption("-h",      &helpOption);
	registerOption("-msx1",   &msx1Option);
	registerOption("-msx2",   &msx2Option);
	registerOption("-msx2+",  &msx2POption);
	registerOption("-turbor", &msxTurboROption);
	registerOption("-config", &configFile);
	registerFileType("xml",   &configFile);
}

void CommandLineParser::registerOption(const std::string &str,
                                       CLIOption* cliOption)
{
	optionMap[str] = cliOption;
}

void CommandLineParser::registerFileType(const std::string &str,
                                         CLIFileType* cliFileType)
{
	fileTypeMap[str] = cliFileType;
}

void CommandLineParser::registerPostConfig(CLIPostConfig *post)
{
	postConfigs.push_back(post);
}

void CommandLineParser::parse(int argc, char **argv)
{
	std::list<std::string> cmdLine;
	for (int i = 1; i < argc; i++) {
		cmdLine.push_back(argv[i]);
	}

	// iterate over all arguments
	while(!cmdLine.empty()) {
		std::string arg = cmdLine.front();
		cmdLine.pop_front();
		
		// first try option
		std::map<std::string, CLIOption*>::const_iterator it;
		it = optionMap.find(arg);
		if (it != optionMap.end()) {
			// parse option
			it->second->parseOption(arg, cmdLine);
			continue;
		}
		
		// then try filename
		int begin = arg.find_last_of('.');
		if (begin != -1) {
			// there is an extension
			int end = arg.find_last_of(',');
			int len = end - begin - 1;
			std::string extension;
			if (len > 0) {
				extension = arg.substr(begin + 1, len);
			} else {
				extension = arg.substr(begin + 1);
			}
			std::map<std::string, CLIFileType*>::const_iterator it;
			it = fileTypeMap.find(extension);
			if (it != fileTypeMap.end()) {
				// parse filetype
				it->second->parseFileType(arg);
				continue;
			}
		}

		// what is this?
		PRT_ERROR("Wrong parameter: " << arg);
	}
	
	// load default config file in case the user didn't specify one
	MSXConfig::Backend *config = MSXConfig::Backend::instance();
	if (!haveConfig) {
		config->loadFile(std::string("msxconfig.xml"));
	}

	// read existing cartridge slots from config
	CartridgeSlotManager::instance()->readConfig();
	
	// execute all postponed options
	std::vector<CLIPostConfig*>::iterator it;
	for (it = postConfigs.begin(); it != postConfigs.end(); it++) {
		(*it)->execute(config);
	}
}


// Help option
void CommandLineParser::HelpOption::parseOption(const std::string &option,
                                                std::list<std::string> &cmdLine)
{
	CommandLineParser* parser = CommandLineParser::instance();
	
	PRT_INFO("OpenMSX " VERSION);
	PRT_INFO("=============");
	PRT_INFO("");
	PRT_INFO("usage: openmsx [arguments]");
	PRT_INFO("  an argument is either an option or a filename");
	PRT_INFO("");
	PRT_INFO("  this is the list of supported options:");
	
	std::map<std::string, CLIOption*>::const_iterator it1;
	for (it1 = parser->optionMap.begin(); it1 != parser->optionMap.end(); it1++) {
		PRT_INFO("    " << it1->first << "\t: " << it1->second->optionHelp());
	}
	
	PRT_INFO("");
	PRT_INFO("  this is the list of supported file types:");
	
	std::map<std::string, CLIFileType*>::const_iterator it2;
	for (it2 = parser->fileTypeMap.begin(); it2 != parser->fileTypeMap.end(); it2++) {
		PRT_INFO("    " << it2->first << "\t: " << it2->second->fileTypeHelp());
	}

	exit(0);
}
const std::string& CommandLineParser::HelpOption::optionHelp()
{
	static const std::string text("Shows this text");
	return text;
}


// Config file type
void CommandLineParser::ConfigFile::parseOption(const std::string &option,
                                                std::list<std::string> &cmdLine)
{
	std::string filename = cmdLine.front();
	cmdLine.pop_front();

	parseFileType(filename);
	
}
const std::string& CommandLineParser::ConfigFile::optionHelp()
{
	static const std::string text("TODO");
	return text;
}
void CommandLineParser::ConfigFile::parseFileType(const std::string &filename)
{
	MSXConfig::Backend *config = MSXConfig::Backend::instance();
	config->loadFile(filename);

	CommandLineParser::instance()->haveConfig = true;
}
const std::string& CommandLineParser::ConfigFile::fileTypeHelp()
{
	static const std::string text("TODO");
	return text;
}

// MSX1
void CommandLineParser::MSX1Option::parseOption(const std::string &option,
                                                std::list<std::string> &cmdLine)
{
	MSXConfig::Backend *config = MSXConfig::Backend::instance();
	config->loadFile("msx1.xml");

	CommandLineParser::instance()->haveConfig = true;
}
const std::string& CommandLineParser::MSX1Option::optionHelp()
{
	static const std::string text("Loads a default MSX 1 configuration");
	return text;
}

// MSX2
void CommandLineParser::MSX2Option::parseOption(const std::string &option,
                                                std::list<std::string> &cmdLine)
{
	MSXConfig::Backend *config = MSXConfig::Backend::instance();
	config->loadFile("msx2.xml");

	CommandLineParser::instance()->haveConfig = true;
}
const std::string& CommandLineParser::MSX2Option::optionHelp()
{
	static const std::string text("Loads a default MSX 2 configuration");
	return text;
}

// MSX2+
void CommandLineParser::MSX2POption::parseOption(const std::string &option,
                                                std::list<std::string> &cmdLine)
{
	MSXConfig::Backend *config = MSXConfig::Backend::instance();
	config->loadFile("msx2plus.xml");

	CommandLineParser::instance()->haveConfig = true;
}
const std::string& CommandLineParser::MSX2POption::optionHelp()
{
	static const std::string text("Loads a default MSX 2 Plus configuration");
	return text;
}

// MSX Turbo R
void CommandLineParser::MSXTurboROption::parseOption(const std::string &option,
                                                std::list<std::string> &cmdLine)
{
	MSXConfig::Backend *config = MSXConfig::Backend::instance();
	config->loadFile("turbor.xml");

	CommandLineParser::instance()->haveConfig = true;
}
const std::string& CommandLineParser::MSXTurboROption::optionHelp()
{
	static const std::string text("Loads a MSX Turbo R configuration");
	return text;
}



/*
	if (isUsed(MBSTEREO)) {
		// Alter subslotting if we need to insert fmpac 
		configureMusMod(std::string("right"));
		configureFmPac(std::string("left"));
	}

addOption(MBSTEREO, "-mbstereo",   false, "Enables -fmpac and -musmod with stereo registration");
*/
