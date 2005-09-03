// $Id$

#include <cassert>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>
#include "ReadDir.hh"
#include "CommandLineParser.hh"
#include "HardwareConfig.hh"
#include "CommandController.hh"
#include "SettingsConfig.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "CliComm.hh"
#include "Version.hh"
#include "MSXRomCLI.hh"
#include "CliExtension.hh"
#include "CassettePlayer.hh"
#include "DiskImageCLI.hh"
#include "ConfigException.hh"
#include "FileException.hh"
#include "EnumSetting.hh"
#include "HostCPU.hh"
#include "MSXMotherBoard.hh"

using std::cout;
using std::endl;
using std::list;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

// class CLIOption

string CLIOption::getArgument(const string& option, list<string>& cmdLine) const
{
	if (cmdLine.empty()) {
		throw FatalError("Missing argument for option \"" + option + "\"");
	}
	string argument = cmdLine.front();
	cmdLine.pop_front();
	return argument;
}

string CLIOption::peekArgument(const list<string>& cmdLine) const
{
	return cmdLine.empty() ? "" : cmdLine.front();
}


// class CommandLineParser

bool CommandLineParser::hiddenStartup = true;

CommandLineParser::CommandLineParser(MSXMotherBoard& motherBoard_)
	: parseStatus(UNPARSED)
	, motherBoard(motherBoard_)
	, hardwareConfig(HardwareConfig::instance())
	, settingsConfig(motherBoard.getCommandController().getSettingsConfig())
	, output(motherBoard.getCliComm())
	, helpOption(*this)
	, versionOption(*this)
	, controlOption(*this)
	, machineOption(*this)
	, settingOption(*this)
	, noMMXOption(*this)
	, noMMXEXTOption(*this)
	, msxRomCLI(new MSXRomCLI(*this))
	, cliExtension(new CliExtension(*this))
	, cassettePlayerCLI(new MSXCassettePlayerCLI(*this))
	, diskImageCLI(new DiskImageCLI(*this))
{
	haveConfig = false;
	haveSettings = false;
	issuedHelp = false;

	registerOption("-machine",  &machineOption, 3);
	registerOption("-setting",  &settingOption, 2);
	registerOption("-h",        &helpOption, 1, 1);
	registerOption("--help",    &helpOption, 1, 1);
	registerOption("-v",        &versionOption, 1, 1);
	registerOption("--version", &versionOption, 1, 1);
	registerOption("-control",  &controlOption, 1, 1);
	#ifdef ASM_X86
	registerOption("-nommx",    &noMMXOption, 1, 1);
	registerOption("-nommxext", &noMMXEXTOption, 1, 1);
	#endif
}

CommandLineParser::~CommandLineParser()
{
}

void CommandLineParser::registerOption(const string &str, CLIOption* cliOption, byte prio, byte length)
{
	OptionData temp;
	temp.option = cliOption;
	temp.prio = prio;
	temp.length = length;
	optionMap[str] = temp;
}

void CommandLineParser::registerFileClass(const string &str,
		CLIFileType* cliFileType)
{
	fileClassMap[str] = cliFileType;
}

void CommandLineParser::postRegisterFileTypes()
{
	const XMLElement* config = settingsConfig.findChild("FileTypes");
	if (config) {
		for (FileClassMap::const_iterator i = fileClassMap.begin();
		     i != fileClassMap.end(); ++i) {
			XMLElement::Children extensions;
			config->getChildren(i->first, extensions);
			for (XMLElement::Children::const_iterator j = extensions.begin();
			     j != extensions.end(); ++j) {
				fileTypeMap[(*j)->getData()] = i->second;
			}
		}
	} else {
		map<string, string> fileExtMap;
		fileExtMap["rom"] = "romimage";
		fileExtMap["dsk"] = "diskimage";
		fileExtMap["di1"] = "diskimage";
		fileExtMap["di2"] = "diskimage";
		fileExtMap["xsa"] = "diskimage";
		fileExtMap["wav"] = "cassetteimage";
		fileExtMap["cas"] = "cassetteimage";
		for (map<string, string>::const_iterator j = fileExtMap.begin();
		     j != fileExtMap.end(); ++j) {
			FileClassMap::const_iterator i = fileClassMap.find(j->second);
			if (i != fileClassMap.end()) {
				fileTypeMap[j->first] = i->second;
			}
		}
	}
}

bool CommandLineParser::parseOption(const string& arg, list<string>& cmdLine, byte priority)
{
	map<string, OptionData>::const_iterator it1 = optionMap.find(arg);
	if (it1 != optionMap.end()) {
		// parse option
		if (it1->second.prio <= priority) {
			return it1->second.option->parseOption(arg, cmdLine);
		}
	}
	return false; // unknown
}

bool CommandLineParser::parseFileName(const string& arg, list<string>& cmdLine)
{
	string originalName(arg);
	try {
		UserFileContext context(motherBoard.getCommandController());
		File file(context.resolve(arg));
		originalName = file.getOriginalName();
	} catch (FileException& e) {
		// ignore
	}
	string::size_type begin = originalName.find_last_of('.');
	if (begin != string::npos) {
		// there is an extension
		string extension = originalName.substr(begin + 1);
		FileTypeMap::const_iterator it2 = fileTypeMap.find(extension);
		if (it2 != fileTypeMap.end()) {
			// parse filetype
			it2->second->parseFileType(arg, cmdLine);
			return true; // file processed
		}
	}
	return false; // unknown
}


void CommandLineParser::parse(int argc, char** argv)
{
	parseStatus = RUN;

	list<string> cmdLine;
	list<string> backupCmdLine;
	for (int i = 1; i < argc; i++) {
		cmdLine.push_back(FileOperations::getConventionalPath(argv[i]));
	}

	for (int priority = 1; priority <= 7; priority++) {
		switch (priority) {
		case 4:
			if (!haveSettings) {
				// Load default settings file in case the user didn't specify
				// one.
				try {
					SystemFileContext context;
					settingsConfig.loadSetting(context, "settings.xml");
				} catch (FileException& e) {
					// settings.xml not found
				} catch (ConfigException& e) {
					throw FatalError("Error in default settings: "
						+ e.getMessage());
				}
				// Consider an attempt to load the settings good enough.
				haveSettings = true;
			}
			postRegisterFileTypes();
			break;
		case 5: {
			createMachineSetting();

			if (!issuedHelp && !haveConfig) {
				// load default config file in case the user didn't specify one
				const string& machine = machineSetting->getValueString();
				output.printInfo("Using default machine: " + machine);
				loadMachine(machine);
				haveConfig = true;
			}
			break;
		}
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
						map<string, OptionData>::const_iterator it1 =
							optionMap.find(arg);
						if (it1 != optionMap.end()) {
							for (int i = 0; i < it1->second.length - 1; ++i) {
								if (!cmdLine.empty()) {
									backupCmdLine.push_back(cmdLine.front());
									cmdLine.pop_front();
								}
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
		throw FatalError(
			"Error parsing command line: " + cmdLine.front() + "\n" +
			"Use \"openmsx -h\" to see a list of available options" );
	}

	hiddenStartup = (parseStatus == CONTROL);
}

CommandLineParser::ParseStatus CommandLineParser::getParseStatus() const
{
	assert(parseStatus != UNPARSED);
	return parseStatus;
}

MSXMotherBoard& CommandLineParser::getMotherBoard() const
{
	return motherBoard;
}

void CommandLineParser::loadMachine(const string& machine)
{
	try {
		hardwareConfig.loadHardware(hardwareConfig, "machines", machine);
	} catch (FileException& e) {
		throw FatalError("Machine \"" + machine + "\" not found (" +
		                 e.getMessage() + ").");
	} catch (ConfigException& e) {
		throw FatalError("Error in \"" + machine + "\" machine (" +
		                 e.getMessage() + ").");
	}
}

static int select(const string& basepath, const struct dirent* d)
{
	// entry must be a directory and must contain the file "hardwareconfig.xml"
	string name = basepath + '/' + d->d_name;
	return FileOperations::isDirectory(name) &&
	       FileOperations::isRegularFile(name + "/hardwareconfig.xml");
}

static void searchMachines(const string& basepath, EnumSetting<int>::Map& machines)
{
	static int unique = 1;
	ReadDir dir(basepath);
	while (dirent* d = dir.getEntry()) {
		if (select(basepath, d)) {
			machines[d->d_name] = unique++; // dummy value
		}
	}
}

void CommandLineParser::createMachineSetting()
{
	EnumSetting<int>::Map machines;

	SystemFileContext context;
	const vector<string>& paths = context.getPaths();
	for (vector<string>::const_iterator it = paths.begin();
	     it != paths.end(); ++it) {
		searchMachines(*it + "machines", machines);
	}

	machines["C-BIOS_MSX2+"] = 0; // default machine

	machineSetting.reset(new EnumSetting<int>(
		motherBoard.getCommandController(), "machine",
		"default machine (takes effect next time openMSX is started)",
		0, machines));
}

// Control option

CommandLineParser::ControlOption::ControlOption(CommandLineParser& parent_)
	: parent(parent_)
{
}

CommandLineParser::ControlOption::~ControlOption()
{
}

bool CommandLineParser::ControlOption::parseOption(const string& option,
		list<string>& cmdLine)
{
	string type_name, arguments;
	StringOp::splitOnFirst(getArgument(option, cmdLine),
	                       ":", type_name, arguments);
	
	map<string, CommandLineParser::ControlType> controlTypeMap;
	controlTypeMap["stdio"] = IO_STD;
#ifdef _WIN32
	controlTypeMap["pipe"] = IO_PIPE;
#endif
	if (controlTypeMap.find(type_name) == controlTypeMap.end()) {
		throw FatalError("Unknown control type: '"  + type_name + "'");
	}
	CommandLineParser::ControlType type = controlTypeMap[type_name];

	parent.getMotherBoard().getCliComm().startInput(type, arguments);
	parent.parseStatus = CONTROL;
	return true;
}

const string& CommandLineParser::ControlOption::optionHelp() const
{
	static const string text("Enable external control of openMSX process");
	return text;
}


// Help option

static string formatSet(const set<string>& inputSet, string::size_type columns)
{
	string outString;
	string::size_type totalLength = 0; // ignore the starting spaces for now
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

static string formatHelptext(const string& helpText,
	string::size_type maxlength, string::size_type indent)
{
	string outText;
	string::size_type index = 0;
	while (helpText.substr(index).length() > maxlength) {
		string::size_type pos = helpText.substr(index).rfind(' ', maxlength);
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

CommandLineParser::HelpOption::HelpOption(CommandLineParser& parent_)
	: parent(parent_)
{
}

CommandLineParser::HelpOption::~HelpOption()
{
}

bool CommandLineParser::HelpOption::parseOption(const string& /*option*/,
		list<string>& /*cmdLine*/)
{
	parent.issuedHelp = true;
	if (!parent.haveSettings) {
		return false; // not parsed yet, load settings first
	}
	cout << Version::FULL_VERSION << endl;
	cout << string(Version::FULL_VERSION.length(), '=') << endl;
	cout << endl;
	cout << "usage: openmsx [arguments]" << endl;
	cout << "  an argument is either an option or a filename" << endl;
	cout << endl;
	cout << "  this is the list of supported options:" << endl;

	map<string, set<string> > optionMap;
	for (map<string, OptionData>::const_iterator it = parent.optionMap.begin();
	     it != parent.optionMap.end(); ++it) {
		optionMap[it->second.option->optionHelp()].insert(it->first);
	}
	printItemMap(optionMap);

	cout << endl;
	cout << "  this is the list of supported file types:" << endl;

	map<string, set<string> > extMap;
	for (FileTypeMap::const_iterator it = parent.fileTypeMap.begin();
	     it != parent.fileTypeMap.end(); ++it) {
		extMap[it->second->fileTypeHelp()].insert(it->first);
	}
	printItemMap(extMap);

	parent.parseStatus = EXIT;
	return true;
}

const string& CommandLineParser::HelpOption::optionHelp() const
{
	static const string text("Shows this text");
	return text;
}


// class VersionOption

CommandLineParser::VersionOption::VersionOption(CommandLineParser& parent_)
	: parent(parent_)
{
}

CommandLineParser::VersionOption::~VersionOption()
{
}

bool CommandLineParser::VersionOption::parseOption(const string& /*option*/,
		list<string>& /*cmdLine*/)
{
	parent.issuedHelp = true;
	cout << Version::FULL_VERSION << endl;
	parent.parseStatus = EXIT;
	return true;
}

const string& CommandLineParser::VersionOption::optionHelp() const
{
	static const string text("Prints openMSX version and exits");
	return text;
}


// Machine option

CommandLineParser::MachineOption::MachineOption(CommandLineParser& parent_)
	: parent(parent_)
{
}

CommandLineParser::MachineOption::~MachineOption()
{
}

bool CommandLineParser::MachineOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	if (parent.haveConfig) {
		throw FatalError("Only one machine option allowed");
	}
	string machine(getArgument(option, cmdLine));
	if (parent.issuedHelp) {
		return true;
	}
	parent.output.printInfo("Using specified machine: " + machine);
	parent.loadMachine(machine);
	parent.haveConfig = true;
	return true;
}
const string& CommandLineParser::MachineOption::optionHelp() const
{
	static const string text("Use machine specified in argument");
	return text;
}


// Setting Option

CommandLineParser::SettingOption::SettingOption(CommandLineParser& parent_)
	: parent(parent_)
{
}

CommandLineParser::SettingOption::~SettingOption()
{
}

bool CommandLineParser::SettingOption::parseOption(const string &option,
		list<string> &cmdLine)
{
	if (parent.haveSettings) {
		throw FatalError("Only one setting option allowed");
	}
	try {
		UserFileContext context(parent.motherBoard.getCommandController(),
		                        "", true); // skip user directories
		parent.settingsConfig.loadSetting(
			context, getArgument(option, cmdLine));
		parent.haveSettings = true;
	} catch (FileException& e) {
		throw FatalError(e.getMessage());
	} catch (ConfigException& e) {
		throw FatalError(e.getMessage());
	}
	return true;
}
const string& CommandLineParser::SettingOption::optionHelp() const
{
	static const string text("Load an alternative settings file");
	return text;
}

// class NoMMXOption

CommandLineParser::NoMMXOption::NoMMXOption(CommandLineParser& parent_)
	: parent(parent_)
{
}

CommandLineParser::NoMMXOption::~NoMMXOption()
{
}

bool CommandLineParser::NoMMXOption::parseOption(const string& /*option*/,
		list<string>& /*cmdLine*/)
{
	cout << "Disabling MMX " << endl;
	HostCPU::getInstance().forceDisableMMX();
	return true;
}

const string& CommandLineParser::NoMMXOption::optionHelp() const
{
	static const string text("Disables usage of MMX, including extensions (for debugging)");
	return text;
}

// class NoMMXEXTOption

CommandLineParser::NoMMXEXTOption::NoMMXEXTOption(CommandLineParser& parent_)
	: parent(parent_)
{
}

CommandLineParser::NoMMXEXTOption::~NoMMXEXTOption()
{
}

bool CommandLineParser::NoMMXEXTOption::parseOption(const string& /*option*/,
		list<string>& /*cmdLine*/)
{
	cout << "Disabling MMX extensions" << endl;
	HostCPU::getInstance().forceDisableMMXEXT();
	return true;
}

const string& CommandLineParser::NoMMXEXTOption::optionHelp() const
{
	static const string text("Disables usage of MMX extensions (for debugging)");
	return text;
}

} // namespace openmsx
