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
#include "FileOperations.hh"
#include "CliCommunicator.hh"

namespace openmsx {

const char* const MACHINE_PATH = "share/machines/";


// class CLIOption

const string CLIOption::getArgument(const string &option, list<string>& cmdLine)
{
	if (cmdLine.empty()) {
		throw FatalError("Missing argument for option \"" + option + "\"");
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
	haveSettings = false;
	issuedHelp = false;

	registerOption("-config",  &configFile,2);
	registerFileType(".xml",   &configFile);
	registerOption("-machine", &machineOption,3);
	registerOption("-setting", &settingOption,2);
	registerOption("-h", &helpOption,1,1); 
}

void CommandLineParser::registerOption(const string &str, CLIOption* cliOption, byte prio, byte length)
{
	OptionData temp;
	temp.option = cliOption;
	temp.prio = prio;
	temp.length = length;
	optionMap[str] = temp;
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
			config->getParametersWithClassClean(extensions);
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

bool CommandLineParser::parseOption(const string &arg,list<string> &cmdLine, byte priority)
{
	map<string, OptionData>::const_iterator it1;
	it1 = optionMap.find(arg);
	if (it1 != optionMap.end()) {
		// parse option
		if (it1->second.prio <= priority) {
			return it1->second.option->parseOption(arg, cmdLine);
		}
	}
	return false; // unknown
}

bool CommandLineParser::parseFileName(const string &arg, list<string>& cmdLine)
{
	unsigned begin = arg.find_last_of('.');
	if (begin != string::npos) {
		// there is an extension
		unsigned end = arg.find_last_of(',');
		string extension;
		if ((end == string::npos) || (end <= begin)) {
			extension = arg.substr(begin + 1);
		} else {
			extension = arg.substr(begin + 1, end - begin - 1);
		}
		if (((extension == "gz") || (extension == "zip")) &&
		    (begin != 0)) {
			end = begin;
			begin = arg.find_last_of('.', begin - 1);
			if (begin != string::npos) {
				extension = arg.substr(begin + 1, end - begin - 1);
			}
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
	list<string> backupCmdLine;
	for (int i = 1; i < argc; i++) {
		cmdLine.push_back(FileOperations::getConventionalPath(argv[i]));
	}

	for (int priority = 1; priority <= 7; priority++) {
		switch (priority) {
		case 4: {
			// load default settings file in case the user didn't specify one
			MSXConfig *config = MSXConfig::instance();
			try {
				if (!settingOption.parsed) {
					SystemFileContext context;
					config->loadSetting(context, "share/settings.xml");
				}
			} catch (MSXException &e) {
				// settings.xml not found
				CliCommunicator::instance().printWarning(
					"No settings file found!");
			}
			haveSettings = true;
			postRegisterFileTypes();
			break;
		}
		case 5:
			if ((!issuedHelp) && (!haveConfig)){
				// load default config file in case the user didn't specify one
				MSXConfig *config = MSXConfig::instance();
				if (!haveConfig) {
					string machine("default");
					try {
						Config *machineConfig =
							config->getConfigById("DefaultMachine");
						if (machineConfig->hasParameter("machine")) {
							machine = machineConfig->getParameter("machine");
							CliCommunicator::instance().printInfo(
								"Using default machine: " + machine);
						}
					} catch (ConfigException &e) {
						// no DefaultMachine section
					}
					try {
						SystemFileContext context;
						config->loadHardware(context,
						MACHINE_PATH + machine + "/hardwareconfig.xml");
					} catch (FileException &e) {
						CliCommunicator::instance().printWarning(
							"No machine file found!");
					}
				}
				haveConfig = true;
			}
			break;
		default:
			// iterate over all arguments
			while (!cmdLine.empty()) {
				string arg = cmdLine.front();
				cmdLine.pop_front();
				// first try options
				if (!parseOption(arg, cmdLine, priority)) {
					// next try the registered filetypes (xml)
					if (!parseFileName(arg, cmdLine)) {
						// no option or known file
						backupCmdLine.push_back(arg);
						map<string, OptionData>::const_iterator it1;
						it1 = optionMap.find(arg);
						if (it1 != optionMap.end()) {
							for (int i = 0; i < it1->second.length - 1; ++i) {
								arg = cmdLine.front();
								cmdLine.pop_front();
								backupCmdLine.push_back(arg);
							}
						}
					}
				}
			}
			cmdLine = backupCmdLine;
			backupCmdLine.clear();
			break;
		}
	}
	if (!cmdLine.empty()) {
		throw FatalError("Error parsing command line: " + cmdLine.front() + "\n" +
		                 "Use \"openmsx -h\" to see a list of available options");
	}
	MSXConfig *config = MSXConfig::instance();
	// read existing cartridge slots from config
	CartridgeSlotManager::instance()->readConfig();
	
	// execute all postponed options
	vector<CLIPostConfig*>::iterator it;
	for (it = postConfigs.begin(); it != postConfigs.end(); it++) {
		(*it)->execute(config);
	}
	optionMap.erase(optionMap.begin(), optionMap.end());
}
// Help option
bool CommandLineParser::HelpOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	CommandLineParser* parser = CommandLineParser::instance();
	parser->issuedHelp = true;
	if (!parser->haveSettings){
		return false; // not parsed yet, load settings first
	}
	cout << "OpenMSX " VERSION << endl;
	cout << "=============" << endl;
	cout << endl;
	cout << "usage: openmsx [arguments]" << endl;
	cout << "  an argument is either an option or a filename" << endl;
	cout << endl;
	cout << "  this is the list of supported options:" << endl;
	
	set<string> printSet;
	map<CLIOption*, set<string>*> tempOptionMap;
	map<CLIOption*, set<string>*>::const_iterator itTempOptionMap;
	set<string>::const_iterator itSet;
	map<string, OptionData>::const_iterator itOption;
	for (itOption = parser->optionMap.begin();
	     itOption != parser->optionMap.end();
	     itOption++) {
		itTempOptionMap = tempOptionMap.find(itOption->second.option);
		if (itTempOptionMap == tempOptionMap.end()) {
			tempOptionMap[itOption->second.option] = new set<string>;
		}
		itTempOptionMap = tempOptionMap.find(itOption->second.option);
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
		cout << (*itSet);
	}
	printSet.clear();
	
	cout << endl;
	cout << "  this is the list of supported file types:" << endl;

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
		cout << *itSet << endl;
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
bool CommandLineParser::ConfigFile::parseOption(const string &option,
		list<string> &cmdLine)
{
	parseFileType(getArgument(option, cmdLine));
	return true;
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
bool CommandLineParser::MachineOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	CommandLineParser * parser = CommandLineParser::instance();
	if ((parser->issuedHelp) || (parser->haveConfig)) return true;
	MSXConfig *config = MSXConfig::instance();
	string machine(getArgument(option, cmdLine));
	SystemFileContext context;
	config->loadHardware(context,
		MACHINE_PATH + machine + "/hardwareconfig.xml");
	CliCommunicator::instance().printInfo(
		"Using specified machine: " + machine);
	parser->haveConfig = true;
	return true;
}
const string& CommandLineParser::MachineOption::optionHelp() const
{
	static const string text("Use machine specified in argument");
	return text;
}


// Setting Option
bool CommandLineParser::SettingOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	CommandLineParser * parser = CommandLineParser::instance();
	if (parser->haveSettings) {
		throw FatalError("Only one setting option allowed");
	}
	parser->haveSettings = true;
	MSXConfig *config = MSXConfig::instance();
	UserFileContext context;
	config->loadSetting(context, getArgument(option, cmdLine));
	return true;
}
const string& CommandLineParser::SettingOption::optionHelp() const
{
	static const string text("Load an alternative settings file");
	return text;
}

} // namespace openmsx
