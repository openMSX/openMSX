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

	registerOption("-config",  &configFile, 2);
	registerFileType(".xml",   &configFile);
	registerOption("-machine", &machineOption, 3);
	registerOption("-setting", &settingOption, 2);
	registerOption("-h",       &helpOption, 1, 1); 
	registerOption("-control", &controlOption, 1, 1); 
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


// Control option

bool CommandLineParser::ControlOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	CliCommunicator::instance().enable();
	return true;
}

const string& CommandLineParser::ControlOption::optionHelp() const
{
	static const string text("Enable external control of openmsx process");
	return text;
}


// Help option

static string formatSet(const set<string>& inputSet, unsigned columns)
{
	string outString;
	unsigned totalLength = 0; // ignore the starting spaces for now
	for (set<string>::const_iterator it = inputSet.begin();
	     it != inputSet.end(); ++it) {
		string temp = *it;
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
	if (totalLength < columns) {
		outString += string(columns - totalLength, ' ');
	}
	return outString;
}

static string formatHelptext(const string& helpText, unsigned maxlength, unsigned indent)
{
	string outText;
	unsigned index = 0;
	while (helpText.substr(index).length() > maxlength) {
		unsigned pos = helpText.substr(index).rfind(' ', maxlength);
		if (pos == string::npos) {
			pos = helpText.substr(maxlength).find(' ');
			if (pos == string::npos) {
				pos = helpText.substr(index).length();
			}
		}
		outText += helpText.substr(index, index + pos) + '\n' +
		           string(indent, ' ');
		index = pos + 1;
	}
	outText += helpText.substr(index);
	return outText;
}

static void printItemMap(const map<string, set<string> >& itemMap)
{
	set<string> printSet;
	for (map<string, set<string> >::const_iterator it = itemMap.begin();
	     it != itemMap.end(); ++it) {
		printSet.insert(formatSet(it->second, 15) + ' ' +
		                formatHelptext(it->first, 50, 20));
	}
	for (set<string>::const_iterator it = printSet.begin();
	     it != printSet.end(); ++it) {
		cout << *it << endl;
	}
}

bool CommandLineParser::HelpOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	CommandLineParser* parser = CommandLineParser::instance();
	parser->issuedHelp = true;
	if (!parser->haveSettings) {
		return false; // not parsed yet, load settings first
	}
	cout << "OpenMSX " VERSION << endl;
	cout << "=============" << endl;
	cout << endl;
	cout << "usage: openmsx [arguments]" << endl;
	cout << "  an argument is either an option or a filename" << endl;
	cout << endl;
	cout << "  this is the list of supported options:" << endl;
	
	map<string, set<string> > optionMap;
	for (map<string, OptionData>::const_iterator it = parser->optionMap.begin();
	     it != parser->optionMap.end(); ++it) {
		optionMap[it->second.option->optionHelp()].insert(it->first);
	}
	printItemMap(optionMap);
	
	cout << endl;
	cout << "  this is the list of supported file types:" << endl;

	map<string, set<string> > extMap;
	for (map<string, CLIFileType*>::const_iterator it = parser->fileTypeMap.begin();
	     it != parser->fileTypeMap.end(); ++it) {
		extMap[it->second->fileTypeHelp()].insert(it->first);
	}
	printItemMap(extMap);
	
	exit(0);
}

const string& CommandLineParser::HelpOption::optionHelp() const
{
	static const string text("Shows this text");
	return text;
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
