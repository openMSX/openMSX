// $Id$

#include <string>
#include <sstream>
#include <cstdio>
#include "CommandLineParser.hh"
#include "libxmlx/xmlx.hh"
#include "MSXConfig.hh"
#include "CartridgeSlotManager.hh"
#include "CliExtension.hh"


CommandLineParser* CommandLineParser::instance()
{
	static CommandLineParser oneInstance;
	return &oneInstance;
}

CommandLineParser::CommandLineParser()
{
	haveConfig = false;

	registerOption("-h",       &helpOption);
	registerOption("-config",  &configFile);
	registerFileType("xml",    &configFile);
	registerOption("-machine", &machineOption);
	registerOption("-setting", &settingOption);
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
	CliExtension extension;
	
	std::list<std::string> cmdLine;
	for (int i = 1; i < argc; i++) {
		cmdLine.push_back(argv[i]);
	}

	// iterate over all arguments
	while(!cmdLine.empty()) {
		std::string arg = cmdLine.front();
		cmdLine.pop_front();
		
		// first try option
		std::map<std::string, CLIOption*>::const_iterator it1;
		it1 = optionMap.find(arg);
		if (it1 != optionMap.end()) {
			// parse option
			it1->second->parseOption(arg, cmdLine);
			continue;
		}
		
		// then try filename
		int begin = arg.find_last_of('.');
		if (begin != -1) {
			// there is an extension
			int end = arg.find_last_of(',');
			std::string extension;
			if ((end == -1) || (end <= begin)) {
				extension = arg.substr(begin + 1);
			} else {
				extension = arg.substr(begin + 1, end - begin - 1);
			}
			std::map<std::string, CLIFileType*>::const_iterator it2;
			it2 = fileTypeMap.find(extension);
			if (it2 != fileTypeMap.end()) {
				// parse filetype
				it2->second->parseFileType(arg);
				continue;
			}
		}

		// what is this?
		PRT_ERROR("Wrong parameter: " << arg);
	}
	
	// load default settings file in case the user didn't specify one
	MSXConfig *config = MSXConfig::instance();
	
	try {
		if (!settingOption.parsed) {
			config->loadFile(new SystemFileContext(), "settings.xml");
		}
	} catch (MSXException &e) {
		// settings.xml not found
		PRT_INFO("Warning: No settings file found!");
	}
	
	// load default config file in case the user didn't specify one
	if (!haveConfig) {
		std::string machine("default");
		try {
			Config *machineConfig =
				config->getConfigById("DefaultMachine");
			if (machineConfig->hasParameter("machine")) {
				machine =
					machineConfig->getParameter("machine");
			}
		} catch (MSXException &e) {
			// no DefaultMachine section
		}
		config->loadFile(new SystemFileContext(),
		                 "machines/" + machine + "/hardwareconfig.xml");
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
const std::string& CommandLineParser::HelpOption::optionHelp() const
{
	static const std::string text("Shows this text");
	return text;
}


// Config file type
void CommandLineParser::ConfigFile::parseOption(const std::string &option,
                                                std::list<std::string> &cmdLine)
{
	parseFileType(cmdLine.front());
	cmdLine.pop_front();
}
const std::string& CommandLineParser::ConfigFile::optionHelp() const
{
	static const std::string text("Use configuration file specified in argument");
	return text;
}
void CommandLineParser::ConfigFile::parseFileType(const std::string &filename)
{
	MSXConfig *config = MSXConfig::instance();
	config->loadFile(new UserFileContext(), filename);

	CommandLineParser::instance()->haveConfig = true;
}
const std::string& CommandLineParser::ConfigFile::fileTypeHelp() const
{
	static const std::string text("Configuration file");
	return text;
}


// Machine option
void CommandLineParser::MachineOption::parseOption(const std::string &option,
                                               std::list<std::string> &cmdLine)
{
	MSXConfig *config = MSXConfig::instance();
	config->loadFile(new SystemFileContext(),
	                 "machines/" + cmdLine.front() + "/hardwareconfig.xml");
	cmdLine.pop_front();
	CommandLineParser::instance()->haveConfig = true;
}
const std::string& CommandLineParser::MachineOption::optionHelp() const
{
	static const std::string text("Use machine specified in argument");
	return text;
}


// Setting Option
void CommandLineParser::SettingOption::parseOption(const std::string &option,
                                                   std::list<std::string> &cmdLine)
{
	if (parsed) {
		PRT_ERROR("Only one setting option allowed");
	}
	parsed = true;
	MSXConfig *config = MSXConfig::instance();
	config->loadFile(new UserFileContext(), cmdLine.front());
	cmdLine.pop_front();
}
const std::string& CommandLineParser::SettingOption::optionHelp() const
{
	static const std::string text("Load an alternative settings file");
	return text;
}
