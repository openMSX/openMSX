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

const std::string CLIOption::getArgument(const std::string &option,
	std::list<std::string> &cmdLine)
{
	if (cmdLine.empty()) {
		PRT_ERROR("Missing argument for option \"" << option <<"\"");
	}
	std::string argument = cmdLine.front();
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

	registerOption("-config",	&configFile);
	registerFileType(".xml",	&configFile);
	registerOption("-machine",	&machineOption);
	registerOption("-setting",	&settingOption);
}

void CommandLineParser::registerOption(const std::string &str,
		CLIOption* cliOption)
{
	optionMap[str] = cliOption;
}

void CommandLineParser::registerFileType(const std::string &str,
		CLIFileType* cliFileType)
{
	if (str[0] == '.'){ // a real extention
		fileTypeMap[str.substr(1)] = cliFileType;
	}
	else{ // or a file class
		fileClassMap[str] = cliFileType;
	}
}

void CommandLineParser::postRegisterFileTypes()
{
	std::list<Config::Parameter*> * extensions;
	std::map<std::string, CLIFileType*,caseltstr>::const_iterator i;
	try{
		Config * config = MSXConfig::instance()->getConfigById("FileTypes");
		for (i=fileClassMap.begin();i!=fileClassMap.end();i++){
			extensions = config->getParametersWithClass((*i).first);
			std::list<Config::Parameter*>::const_iterator j;
			for (j=extensions->begin();j!=extensions->end();j++){
				fileTypeMap[(*j)->value]=(*i).second;
			}
		}
	}catch (MSXException &e) {
		std::map<std::string,std::string> fileExtMap;
		std::map<std::string,std::string>::const_iterator j;
		fileExtMap["rom"]="romimages";
		fileExtMap["dsk"]="diskimages";
		fileExtMap["di1"]="diskimages";
		fileExtMap["di2"]="diskimages";
		fileExtMap["xsa"]="diskimages";
		fileExtMap["cas"]="cassetteimages";
		fileExtMap["wav"]="cassettesounds";
		for (j=fileExtMap.begin();j!=fileExtMap.end();j++){	
			i=fileClassMap.find((*j).second);
			if (i!=fileClassMap.end()){
				fileTypeMap[(*j).first]=(*i).second;
			}
		}	
	}		
}

bool CommandLineParser::parseOption(const std::string &arg,std::list<std::string> &cmdLine)
{
	std::map<std::string, CLIOption*>::const_iterator it1;
	it1 = optionMap.find(arg);
	if (it1 != optionMap.end()) {
		// parse option
		it1->second->parseOption(arg, cmdLine);
		return true; // processed
	}	
	return false; // unknown
}

bool CommandLineParser::parseFileName(const std::string &arg,std::list<std::string> &cmdLine)
{
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
	
	std::list<std::string> cmdLine;
	std::list<std::string> fileCmdLine;
	for (int i = 1; i < argc; i++) {
		cmdLine.push_back(argv[i]);
	}

	// iterate over all arguments
	while (!cmdLine.empty()) {
		std::string arg = cmdLine.front();
		cmdLine.pop_front();
				
		// first try options
		if (!parseOption(arg,cmdLine)){
			// next try the registered filetypes (xml)
			if (!parseFileName(arg,cmdLine)){
				fileCmdLine.push_back(arg); // no option or known file
			}	
		}	
	}	
	
	// load default settings file in case the user didn't specify one
	MSXConfig *config = MSXConfig::instance();
	
	try {
		if (!settingOption.parsed) {
			config->loadFile(new SystemFileContext(), "share/settings.xml");
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
		try{		
		std::cout << "Selected: " << MACHINE_PATH + machine << std::endl;
		config->loadFile(new SystemFileContext(),
		        MACHINE_PATH + machine + "/hardwareconfig.xml");
		} catch (FileException &e) {
		bool found=false;
		std::list<std::string>::const_iterator it;
		for (it = fileCmdLine.begin();it !=	fileCmdLine.end();it ++){
			if (*it == "-h"){
				found = true;	
			}
		}
		if (!found){
			PRT_INFO("Warning: No machine file found!");	
		}
		}		
	}

	postRegisterFileTypes();
	registerOption("-h", &helpOption); // AFTER registering filetypes
	
	// now that the configs are loaded, try filenames
	while (!fileCmdLine.empty()) {
		std::string arg = fileCmdLine.front();
		fileCmdLine.pop_front();
		// first, try the options
		if (!parseOption(arg,cmdLine)){
			// next try the registered filetypes (xml)
			if (!parseFileName(arg,cmdLine)){
				// what is this?
				PRT_ERROR("Wrong parameter: " << arg);
			}	
		}	
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
	
	std::set<std::string> printSet;
	std::map<CLIOption*,std::set<std::string>*> tempOptionMap;
	std::map<CLIOption*,std::set<std::string>*>::iterator itTempOptionMap;
	std::set<std::string>::iterator itSet;
	std::map<std::string, CLIOption*>::const_iterator itOption;
	for (itOption = parser->optionMap.begin(); itOption != parser->optionMap.end(); itOption++) {
		itTempOptionMap = tempOptionMap.find(itOption->second);
		if (itTempOptionMap == tempOptionMap.end()){
		tempOptionMap[itOption->second]=new std::set<std::string>;
		}
		itTempOptionMap = tempOptionMap.find(itOption->second);
		if (itTempOptionMap != tempOptionMap.end()){	
			itTempOptionMap->second->insert(itOption->first);
		}			
	}
	std::string tempString;	
	for (itTempOptionMap=tempOptionMap.begin();itTempOptionMap != tempOptionMap.end();itTempOptionMap++){
		tempString = formatSet(itTempOptionMap->second,15) + " " 
		+ formatHelptext(itTempOptionMap->first->optionHelp() ,50,20);
		printSet.insert (tempString);
		delete itTempOptionMap->second;
	}
	for (itSet = printSet.begin(); itSet!=printSet.end();itSet++){
	PRT_INFO(*itSet);
	}	
	printSet.clear();	
		
	PRT_INFO("");
	PRT_INFO("  this is the list of supported file types:");

	std::map<CLIFileType*,std::set<std::string>*> tempExtMap;
	std::map<CLIFileType*,std::set<std::string>*>::iterator itTempExtMap;
	std::map<std::string, CLIFileType*>::const_iterator itFileType;
	for (itFileType = parser->fileTypeMap.begin(); itFileType != parser->fileTypeMap.end(); itFileType++) {
		itTempExtMap = tempExtMap.find(itFileType->second);
		if (itTempExtMap == tempExtMap.end()){
		tempExtMap[itFileType->second]=new std::set<std::string>;
		}
		itTempExtMap = tempExtMap.find(itFileType->second);
		if (itTempExtMap != tempExtMap.end()){	
			itTempExtMap->second->insert(itFileType->first);
		}			
	}
	for (itTempExtMap=tempExtMap.begin();itTempExtMap != tempExtMap.end();itTempExtMap++){
		tempString = formatSet(itTempExtMap->second,15) + " " 
		+ formatHelptext(itTempExtMap->first->fileTypeHelp(),50,20);
		printSet.insert (tempString);
		delete itTempExtMap->second; 
	}
	for (itSet = printSet.begin(); itSet!=printSet.end();itSet++){
	PRT_INFO(*itSet);
	}
	exit(0);
}
const std::string& CommandLineParser::HelpOption::optionHelp() const
{
	static const std::string text("Shows this text");
	return text;
}

std::string CommandLineParser::HelpOption::formatSet(std::set<std::string> * inputSet,unsigned columns){
		std::string fillString (60,' ');
		std::set<std::string>::iterator it4;
		std::string outString="";
		unsigned totalLength=0; // ignore the starting spaces for now
		for (it4 = inputSet->begin(); it4!=inputSet->end();it4++){
			std::string temp = *it4;
					
			if (totalLength == 0){	// first element ?
				outString += "    " + temp;
				totalLength = temp.length();	
			}
			else{
				outString += ", ";	
				if ((totalLength + temp.length()) > columns){
					outString += "\n    " + temp;
					totalLength = temp.length();
				}	
				else{
					outString += temp;
					totalLength += 2 + temp.length();
				}		
			}
		}
		if (totalLength <= columns){
			outString += fillString.substr(60-(columns-totalLength));
		}
	return outString;
}

std::string CommandLineParser::HelpOption::formatHelptext(std::string helpText,unsigned maxlength, unsigned indent){
	std::string outText="";
	unsigned pos;
	std::string fillString (60,' ');
	unsigned index=0;
	while (helpText.substr(index).length() > maxlength){
		pos=helpText.substr(index).rfind(' ',index+maxlength);
		if (!pos){
		pos=helpText.substr(index+maxlength).find(' ');
		}
		if (pos){
			outText += helpText.substr(index,index+pos) + "\n" + fillString.substr(60-indent);
			index=pos+1;
		}
	}	
	outText += helpText.substr(index);
		
	return outText;
}	

// Config file type
void CommandLineParser::ConfigFile::parseOption(const std::string &option,
		std::list<std::string> &cmdLine)
{
	parseFileType(getArgument(option, cmdLine));
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
		MACHINE_PATH + getArgument(option, cmdLine) +
			 "/hardwareconfig.xml");
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
	config->loadFile(new UserFileContext(), getArgument(option, cmdLine));
}
const std::string& CommandLineParser::SettingOption::optionHelp() const
{
	static const std::string text("Load an alternative settings file");
	return text;
}
