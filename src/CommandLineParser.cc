// $Id$

#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <set>
#include "CommandLineParser.hh"
#include "libxmlx/xmlx.hh"
#include "MSXConfig.hh"
#include "CartridgeSlotManager.hh"
#include "CliExtension.hh"
#include "File.hh"

const char* const MACHINE_PATH = "share/machines/";


// class CLIOption

const string CLIOption::getArgument(const string &option, list<string>& cmdLine)
{
	if (cmdLine.empty()) {
		PRT_ERROR("Missing argument for option \"" << option <<"\"");
	}
	string argument = cmdLine.front();
	cmdLine.pop_front();
	return argument;
}


// class CommandLineParser

CommandLineParser* CommandLineParser::instance()
{
	static CommandLineParser oneInstance;
	return &oneInstance;
}

CommandLineParser::CommandLineParser()
{
	haveConfig = false;

	registerOption("-config",  &configFile);
	registerFileType(".xml",   &configFile);
	registerOption("-machine", &machineOption);
	registerOption("-setting", &settingOption);
}

void CommandLineParser::registerOption(const string &str, CLIOption* cliOption)
{
	optionMap[str] = cliOption;
}

void CommandLineParser::registerFileType(const string &str,
		CLIFileType* cliFileType)
{
	if (str[0] == '.') {
		// a real extention
		fileTypeMap[str.substr(1)] = cliFileType;
	} else {
		// or a file class
		fileClassMap[str] = cliFileType;
	}
}

void CommandLineParser::postRegisterFileTypes()
{
	list<Config::Parameter*> *extensions;
	map<string, CLIFileType*, caseltstr>::const_iterator i;
	try {
		Config *config = MSXConfig::instance()->getConfigById("FileTypes");
		for (i = fileClassMap.begin(); i != fileClassMap.end(); i++) {
			extensions = config->getParametersWithClass(i->first);
			list<Config::Parameter*>::const_iterator j;
			for (j = extensions->begin(); j != extensions->end(); j++) {
				fileTypeMap[(*j)->value] = i->second;
			}
		}
	} catch (ConfigException &e) {
		map<string,string> fileExtMap;
		map<string,string>::const_iterator j;
		fileExtMap["rom"] = "romimages";
		fileExtMap["dsk"] = "diskimages";
		fileExtMap["di1"] = "diskimages";
		fileExtMap["di2"] = "diskimages";
		fileExtMap["xsa"] = "diskimages";
		fileExtMap["cas"] = "cassetteimages";
		fileExtMap["wav"] = "cassettesounds";
		for (j = fileExtMap.begin(); j != fileExtMap.end(); j++) {
			i = fileClassMap.find((*j).second);
			if (i != fileClassMap.end()) {
				fileTypeMap[j->first] = i->second;
			}
		}
	}
}

bool CommandLineParser::parseOption(const string &arg,list<string> &cmdLine)
{
	map<string, CLIOption*>::const_iterator it1;
	it1 = optionMap.find(arg);
	if (it1 != optionMap.end()) {
		// parse option
		it1->second->parseOption(arg, cmdLine);
		return true; // processed
	}
	return false; // unknown
}

bool CommandLineParser::parseFileName(const string &arg, list<string>& cmdLine)
{
	int begin = arg.find_last_of('.');
	if (begin != -1) {
		// there is an extension
		int end = arg.find_last_of(',');
		string extension;
		if ((end == -1) || (end <= begin)) {
			extension = arg.substr(begin + 1);
		} else {
			extension = arg.substr(begin + 1, end - begin - 1);
		}
		map<string, CLIFileType*>::const_iterator it2;
		it2 = fileTypeMap.find(extension);
		if (it2 != fileTypeMap.end()) {
			// parse filetype
			it2->second->parseFileType(arg);
			return true; // file processed
		}
	}
	return false; // unknown
}


void CommandLineParser::registerPostConfig(CLIPostConfig *post)
{
	postConfigs.push_back(post);
}

void CommandLineParser::parse(int argc, char **argv)
{
	CliExtension extension;
	
	list<string> cmdLine;
	list<string> fileCmdLine;
	for (int i = 1; i < argc; i++) {
		cmdLine.push_back(argv[i]);
	}

	// iterate over all arguments
	while (!cmdLine.empty()) {
		string arg = cmdLine.front();
		cmdLine.pop_front();
		
		// first try options
		if (!parseOption(arg, cmdLine)) {
			// next try the registered filetypes (xml)
			if (!parseFileName(arg, cmdLine)) {
				fileCmdLine.push_back(arg); // no option or known file
			}
		}
	}
	
	// load default settings file in case the user didn't specify one
	MSXConfig *config = MSXConfig::instance();
	try {
		if (!settingOption.parsed) {
			SystemFileContext context;
			config->loadSetting(context, "share/settings.xml");
		}
	} catch (MSXException &e) {
		// settings.xml not found
		PRT_INFO("Warning: No settings file found!");
	}
	
	// load default config file in case the user didn't specify one
	if (!haveConfig) {
		string machine("default");
		try {
			Config *machineConfig =
				config->getConfigById("DefaultMachine");
			if (machineConfig->hasParameter("machine")) {
				machine =
					machineConfig->getParameter("machine");
				PRT_INFO("Using default machine: " << machine);
			}
		} catch (ConfigException &e) {
			// no DefaultMachine section
		}
		try {
			SystemFileContext context;
			config->loadHardware(context,
				MACHINE_PATH + machine + "/hardwareconfig.xml");
		} catch (FileException &e) {
			bool found = false;
			list<string>::const_iterator it;
			for (it = fileCmdLine.begin(); it != fileCmdLine.end(); it++) {
				if (*it == "-h") {
					found = true;
				}
			}
			if (!found) {
				PRT_INFO("Warning: No machine file found!");
			}
		}
	}

	postRegisterFileTypes();
	registerOption("-h", &helpOption); // AFTER registering filetypes
	
	// now that the configs are loaded, try filenames
	while (!fileCmdLine.empty()) {
		string arg = fileCmdLine.front();
		fileCmdLine.pop_front();
		// first, try the options
		if (!parseOption(arg, cmdLine)) {
			// next try the registered filetypes (xml)
			if (!parseFileName(arg, cmdLine)) {
				// what is this?
				PRT_ERROR("Wrong parameter: " << arg);
			}
		}
	}
	
	// read existing cartridge slots from config
	CartridgeSlotManager::instance()->readConfig();
	
	// execute all postponed options
	vector<CLIPostConfig*>::iterator it;
	for (it = postConfigs.begin(); it != postConfigs.end(); it++) {
		(*it)->execute(config);
	}
}


// Help option
void CommandLineParser::HelpOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	CommandLineParser* parser = CommandLineParser::instance();
	
	
	PRT_INFO("OpenMSX " VERSION);
	PRT_INFO("=============");
	PRT_INFO("");
	PRT_INFO("usage: openmsx [arguments]");
	PRT_INFO("  an argument is either an option or a filename");
	PRT_INFO("");
	PRT_INFO("  this is the list of supported options:");
	
	set<string> printSet;
	map<CLIOption*, set<string>*> tempOptionMap;
	map<CLIOption*, set<string>*>::const_iterator itTempOptionMap;
	set<string>::const_iterator itSet;
	map<string, CLIOption*>::const_iterator itOption;
	for (itOption = parser->optionMap.begin();
	     itOption != parser->optionMap.end();
	     itOption++) {
		itTempOptionMap = tempOptionMap.find(itOption->second);
		if (itTempOptionMap == tempOptionMap.end()) {
			tempOptionMap[itOption->second] = new set<string>;
		}
		itTempOptionMap = tempOptionMap.find(itOption->second);
		if (itTempOptionMap != tempOptionMap.end()) {
			itTempOptionMap->second->insert(itOption->first);
		}
	}
	string tempString;
	for (itTempOptionMap = tempOptionMap.begin();
	     itTempOptionMap != tempOptionMap.end();
	     itTempOptionMap++) {
		tempString = formatSet(itTempOptionMap->second,15) + " " +
			formatHelptext(itTempOptionMap->first->optionHelp(), 50, 20);
		printSet.insert(tempString);
		delete itTempOptionMap->second;
	}
	for (itSet = printSet.begin(); itSet != printSet.end(); itSet++) {
		PRT_INFO(*itSet);
	}
	printSet.clear();
	
	PRT_INFO("");
	PRT_INFO("  this is the list of supported file types:");

	map<CLIFileType*, set<string>*> tempExtMap;
	map<CLIFileType*, set<string>*>::const_iterator itTempExtMap;
	map<string, CLIFileType*>::const_iterator itFileType;
	for (itFileType = parser->fileTypeMap.begin();
	     itFileType != parser->fileTypeMap.end();
	     itFileType++) {
		itTempExtMap = tempExtMap.find(itFileType->second);
		if (itTempExtMap == tempExtMap.end()) {
			tempExtMap[itFileType->second] = new set<string>;
		}
		itTempExtMap = tempExtMap.find(itFileType->second);
		if (itTempExtMap != tempExtMap.end()) {
			itTempExtMap->second->insert(itFileType->first);
		}
	}
	for (itTempExtMap = tempExtMap.begin();
	     itTempExtMap != tempExtMap.end();
	     itTempExtMap++) {
		tempString = formatSet(itTempExtMap->second, 15) + " " +
			formatHelptext(itTempExtMap->first->fileTypeHelp(), 50, 20);
		printSet.insert(tempString);
		delete itTempExtMap->second; 
	}
	for (itSet = printSet.begin(); itSet != printSet.end(); itSet++) {
		PRT_INFO(*itSet);
	}
	exit(0);
}

const string& CommandLineParser::HelpOption::optionHelp() const
{
	static const string text("Shows this text");
	return text;
}

string CommandLineParser::HelpOption::formatSet(set<string> *inputSet,
                                                     unsigned columns)
{
	string fillString(60, ' ');
	set<string>::iterator it4;
	string outString;
	unsigned totalLength = 0; // ignore the starting spaces for now
	for (it4 = inputSet->begin(); it4 != inputSet->end(); it4++) {
		string temp = *it4;
		if (totalLength == 0) {
			// first element ?
			outString += "    " + temp;
			totalLength = temp.length();
		} else {
			outString += ", ";
			if ((totalLength + temp.length()) > columns) {
				outString += "\n    " + temp;
				totalLength = temp.length();
			} else {
				outString += temp;
				totalLength += 2 + temp.length();
			}
		}
	}
	if (totalLength <= columns) {
		outString += fillString.substr(60 - (columns - totalLength));
	}
	return outString;
}

string CommandLineParser::HelpOption::formatHelptext(string helpText,
                   unsigned maxlength, unsigned indent)
{
	string outText;
	unsigned pos;
	string fillString(60, ' ');
	unsigned index = 0;
	while (helpText.substr(index).length() > maxlength) {
		pos = helpText.substr(index).rfind(' ', index+maxlength);
		if (!pos) {
			pos = helpText.substr(index + maxlength).find(' ');
		}
		if (pos) {
			outText += helpText.substr(index, index + pos) + 
			           "\n" + fillString.substr(60 - indent);
			index = pos + 1;
		}
	}
	outText += helpText.substr(index);
	return outText;
}

// Config file type
void CommandLineParser::ConfigFile::parseOption(const string &option,
		list<string> &cmdLine)
{
	parseFileType(getArgument(option, cmdLine));
}
const string& CommandLineParser::ConfigFile::optionHelp() const
{
	static const string text("Use configuration file specified in argument");
	return text;
}
void CommandLineParser::ConfigFile::parseFileType(const string &filename)
{
	MSXConfig *config = MSXConfig::instance();
	UserFileContext context;
	config->loadHardware(context, filename);

	CommandLineParser::instance()->haveConfig = true;
}
const string& CommandLineParser::ConfigFile::fileTypeHelp() const
{
	static const string text("Configuration file");
	return text;
}


// Machine option
void CommandLineParser::MachineOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	MSXConfig *config = MSXConfig::instance();
	string machine(getArgument(option, cmdLine));
	SystemFileContext context;
	config->loadHardware(context,
		MACHINE_PATH + machine + "/hardwareconfig.xml");
	PRT_INFO("Using specified machine: " << machine);
	CommandLineParser::instance()->haveConfig = true;
}
const string& CommandLineParser::MachineOption::optionHelp() const
{
	static const string text("Use machine specified in argument");
	return text;
}


// Setting Option
void CommandLineParser::SettingOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	if (parsed) {
		PRT_ERROR("Only one setting option allowed");
	}
	parsed = true;
	MSXConfig *config = MSXConfig::instance();
	UserFileContext context;
	config->loadSetting(context, getArgument(option, cmdLine));
}
const string& CommandLineParser::SettingOption::optionHelp() const
{
	static const string text("Load an alternative settings file");
	return text;
}
