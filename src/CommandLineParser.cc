// $Id$

#include "CommandLineParser.hh"
#include "CLIOption.hh"
#include "GlobalCommandController.hh"
#include "SettingsConfig.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "GlobalCliComm.hh"
#include "StdioMessages.hh"
#include "Version.hh"
#include "MSXRomCLI.hh"
#include "CliExtension.hh"
#include "CliConnection.hh"
#include "CassettePlayerCLI.hh"
#include "DiskImageCLI.hh"
#include "HDImageCLI.hh"
#include "CDImageCLI.hh"
#include "ConfigException.hh"
#include "FileException.hh"
#include "EnumSetting.hh"
#include "XMLLoader.hh"
#include "XMLElement.hh"
#include "XMLException.hh"
#include "HostCPU.hh"
#include "StringOp.hh"
#include "utf8_checked.hh"
#include "GLUtil.hh"
#include "Reactor.hh"
#include "build-info.hh"
#include "components.hh"

#if COMPONENT_LASERDISC
#include "LaserdiscPlayerCLI.hh"
#endif

#include <cassert>
#include <iostream>
#include <cstdio>

using std::cout;
using std::endl;
using std::deque;
using std::map;
using std::set;
using std::string;
using std::vector;

#ifdef _WIN32
using namespace utf8;
#endif

namespace openmsx {

class HelpOption : public CLIOption
{
public:
	explicit HelpOption(CommandLineParser& parser);
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
private:
	CommandLineParser& parser;
};

class VersionOption : public CLIOption
{
public:
	explicit VersionOption(CommandLineParser& parser);
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
private:
	CommandLineParser& parser;
};

class ControlOption : public CLIOption
{
public:
	explicit ControlOption(CommandLineParser& parser);
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
private:
	CommandLineParser& parser;
};

class ScriptOption : public CLIOption
{
public:
	const CommandLineParser::Scripts& getScripts() const;
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
private:
	CommandLineParser::Scripts scripts;
};

class MachineOption : public CLIOption
{
public:
	explicit MachineOption(CommandLineParser& parser);
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
private:
	CommandLineParser& parser;
};

class SettingOption : public CLIOption
{
public:
	explicit SettingOption(CommandLineParser& parser);
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
private:
	CommandLineParser& parser;
};

class NoMMXOption : public CLIOption
{
public:
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
};

class NoSSEOption : public CLIOption {
public:
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
};

class NoSSE2Option : public CLIOption {
public:
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
};

class NoPBOOption : public CLIOption {
public:
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
};

class TestConfigOption : public CLIOption
{
public:
	explicit TestConfigOption(CommandLineParser& parser);
	virtual bool parseOption(const string& option, deque<string>& cmdLine);
	virtual const string& optionHelp() const;
private:
	CommandLineParser& parser;
};


// class CommandLineParser

CommandLineParser::CommandLineParser(Reactor& reactor_)
	: reactor(reactor_)
	, settingsConfig(reactor.getGlobalCommandController().getSettingsConfig())
	, output(reactor.getCliComm())
	, helpOption(new HelpOption(*this))
	, versionOption(new VersionOption(*this))
	, controlOption(new ControlOption(*this))
	, scriptOption(new ScriptOption())
	, machineOption(new MachineOption(*this))
	, settingOption(new SettingOption(*this))
	, noMMXOption(new NoMMXOption())
	, noSSEOption(new NoSSEOption())
	, noSSE2Option(new NoSSE2Option())
	, noPBOOption(new NoPBOOption())
	, testConfigOption(new TestConfigOption(*this))
	, msxRomCLI(new MSXRomCLI(*this))
	, cliExtension(new CliExtension(*this))
	, cassettePlayerCLI(new CassettePlayerCLI(*this))
#if COMPONENT_LASERDISC
	, laserdiscPlayerCLI(new LaserdiscPlayerCLI(*this))
#endif
	, diskImageCLI(new DiskImageCLI(*this))
	, hdImageCLI(new HDImageCLI(*this))
	, cdImageCLI(new CDImageCLI(*this))
	, parseStatus(UNPARSED)
{
	haveConfig = false;
	haveSettings = false;

	registerOption("-machine",    *machineOption, 4);
	registerOption("-setting",    *settingOption, 2);
	registerOption("-h",          *helpOption, 1, 1);
	registerOption("--help",      *helpOption, 1, 1);
	registerOption("-v",          *versionOption, 1, 1);
	registerOption("--version",   *versionOption, 1, 1);
	registerOption("-control",    *controlOption, 1, 1);
	registerOption("-script",     *scriptOption, 1, 1);
	#if ASM_X86
	registerOption("-nommx",      *noMMXOption, 1, 1);
	registerOption("-nosse",      *noSSEOption, 1, 1);
	registerOption("-nosse2",     *noSSE2Option, 1, 1);
	#endif
	#if COMPONENT_GL
	registerOption("-nopbo",      *noPBOOption, 1, 1);
	#endif
	registerOption("-testconfig", *testConfigOption, 1, 1);

	registerFileTypes();
}

CommandLineParser::~CommandLineParser()
{
}

void CommandLineParser::registerOption(
	const char* str, CLIOption& cliOption, unsigned prio, unsigned length)
{
	OptionData temp;
	temp.option = &cliOption;
	temp.prio = prio;
	temp.length = length;
	optionMap[str] = temp;
}

void CommandLineParser::registerFileClass(
	const char* str, CLIFileType& cliFileType)
{
	fileClassMap[str] = &cliFileType;
}

void CommandLineParser::registerFileTypes()
{
	if (const XMLElement* config =
			settingsConfig.getXMLElement().findChild("FileTypes")) {
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
		fileExtMap["ri"]  = "romimage";
		fileExtMap["dsk"] = "diskimage";
		fileExtMap["di1"] = "diskimage";
		fileExtMap["di2"] = "diskimage";
		fileExtMap["xsa"] = "diskimage";
		fileExtMap["wav"] = "cassetteimage";
		fileExtMap["cas"] = "cassetteimage";
		fileExtMap["ogv"] = "laserdiscimage";
		for (map<string, string>::const_iterator j = fileExtMap.begin();
		     j != fileExtMap.end(); ++j) {
			FileClassMap::const_iterator i = fileClassMap.find(j->second);
			if (i != fileClassMap.end()) {
				fileTypeMap[j->first] = i->second;
			}
		}
	}
}

bool CommandLineParser::parseOption(
	const string& arg, deque<string>& cmdLine, unsigned priority)
{
	map<string, OptionData>::const_iterator it1 = optionMap.find(arg);
	if (it1 != optionMap.end()) {
		// parse option
		if (it1->second.prio <= priority) {
			try {
				return it1->second.option->parseOption(
					arg, cmdLine);
			} catch (MSXException& e) {
				throw FatalError(e.getMessage());
			}
		}
	}
	return false; // unknown
}

bool CommandLineParser::parseFileName(const string& arg, deque<string>& cmdLine)
{
	string originalName(arg);
	try {
		UserFileContext context;
		File file(context.resolve(getGlobalCommandController(), arg));
		originalName = file.getOriginalName();
	} catch (FileException&) {
		// ignore
	}
	string::size_type begin = originalName.find_last_of('.');
	if (begin != string::npos) {
		// there is an extension
		string extension = originalName.substr(begin + 1);
		FileTypeMap::const_iterator it2 = fileTypeMap.find(extension);
		if (it2 != fileTypeMap.end()) {
			try {
				// parse filetype
				it2->second->parseFileType(arg, cmdLine);
				return true; // file processed
			} catch (MSXException& e) {
				throw FatalError(e.getMessage());
			}
		}
	}
	return false; // unknown
}


void CommandLineParser::parse(int argc, char** argv)
{
	parseStatus = RUN;

	deque<string> cmdLine;
	deque<string> backupCmdLine;
	for (int i = 1; i < argc; i++) {
		cmdLine.push_back(FileOperations::getConventionalPath(argv[i]));
	}

	for (int priority = 1; (priority <= 8) && (parseStatus != EXIT); ++priority) {
		switch (priority) {
		case 2: // after ControlOption has been parsed
			if (parseStatus != CONTROL) {
				// if there already is a XML-StdioConnection, we
				// can't also show plain messages on stdout
				GlobalCliComm& cliComm = reactor.getGlobalCliComm();
				std::auto_ptr<CliListener> listener(
					new StdioMessages());
				cliComm.addListener(listener);
			}
		case 3:
			if (!haveSettings) {
				// Load default settings file in case the user
				// didn't specify one.
				SystemFileContext context;
				string filename = "settings.xml";
				try {
					settingsConfig.loadSetting(context, filename);
				} catch (XMLException& e) {
					output.printWarning(
						"Loading of settings failed: " +
						e.getMessage() + "\n"
						"Reverting to default settings.");
				} catch (FileException&) {
					// settings.xml not found
				} catch (ConfigException& e) {
					throw FatalError("Error in default settings: "
						+ e.getMessage());
				}
				// Consider an attempt to load the settings good enough.
				haveSettings = true;
				// Even if parsing failed, use this file for saving,
				// this forces overwriting a non-setting file.
				settingsConfig.setSaveFilename(context, filename);
			}
			break;
		case 5: {
			if (!haveConfig) {
				// load default config file in case the user didn't specify one
				const string& machine =
					reactor.getMachineSetting().getValueString();
				try {
					reactor.switchMachine(machine);
				} catch (MSXException& e) {
					output.printInfo(
						"Failed to initialize default machine: " + e.getMessage()
						);
					// Default machine is broken; fall back to C-BIOS config.
					const string& fallbackMachine =
						reactor.getMachineSetting().getRestoreValueString();
					output.printInfo("Using fallback machine: " + fallbackMachine);
					try {
						reactor.switchMachine(fallbackMachine);
					} catch (MSXException& e2) {
						// Fallback machine failed as well; we're out of options.
						throw FatalError(e2.getMessage());
					}
				}
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
					if ((priority < 7) ||
					    !parseFileName(arg, cmdLine)) {
						// no option or known file
						backupCmdLine.push_back(arg);
						map<string, OptionData>::const_iterator it1 =
							optionMap.find(arg);
						if (it1 != optionMap.end()) {
							for (unsigned i = 0; i < it1->second.length - 1; ++i) {
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
	if (!cmdLine.empty() && (parseStatus != EXIT)) {
		throw FatalError(
			"Error parsing command line: " + cmdLine.front() + "\n" +
			"Use \"openmsx -h\" to see a list of available options" );
	}

	hiddenStartup = (parseStatus == CONTROL || parseStatus == TEST );
}

bool CommandLineParser::isHiddenStartup() const
{
	return hiddenStartup;
}

CommandLineParser::ParseStatus CommandLineParser::getParseStatus() const
{
	assert(parseStatus != UNPARSED);
	return parseStatus;
}

const CommandLineParser::Scripts& CommandLineParser::getStartupScripts() const
{
	return scriptOption->getScripts();
}

MSXMotherBoard* CommandLineParser::getMotherBoard() const
{
	return reactor.getMotherBoard();
}

GlobalCommandController& CommandLineParser::getGlobalCommandController() const
{
	return reactor.getGlobalCommandController();
}


// Control option

ControlOption::ControlOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool ControlOption::parseOption(const string& option, deque<string>& cmdLine)
{
	string fullType = getArgument(option, cmdLine);
	string type, arguments;
	StringOp::splitOnFirst(fullType, ':', type, arguments);

	CommandController& controller = parser.getGlobalCommandController();
	EventDistributor& distributor = parser.reactor.getEventDistributor();
	GlobalCliComm& cliComm        = parser.reactor.getGlobalCliComm();
	std::auto_ptr<CliListener> connection;
	if (type == "stdio") {
		connection.reset(new StdioConnection(
			controller, distributor));
#ifdef _WIN32
	} else if (type == "pipe") {
		OSVERSIONINFO info;
		info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&info);
		if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			connection.reset(new PipeConnection(
				controller, distributor, arguments));
		} else {
			throw FatalError("Pipes are not supported on this "
			                 "version of Windows");
		}
#endif
	} else {
		throw FatalError("Unknown control type: '"  + type + '\'');
	}
	cliComm.addListener(connection);

	parser.parseStatus = CommandLineParser::CONTROL;
	return true;
}

const string& ControlOption::optionHelp() const
{
	static const string text("Enable external control of openMSX process");
	return text;
}


// Script option

const CommandLineParser::Scripts& ScriptOption::getScripts() const
{
	return scripts;
}

bool ScriptOption::parseOption(const string& option, deque<string>& cmdLine)
{
	scripts.push_back(getArgument(option, cmdLine));
	return true;
}

const string& ScriptOption::optionHelp() const
{
	static const string text("Run extra startup script");
	return text;
}


// Help option

static string formatSet(const set<string>& inputSet, string::size_type columns)
{
	StringOp::Builder outString;
	string::size_type totalLength = 0; // ignore the starting spaces for now
	for (set<string>::const_iterator it = inputSet.begin();
	     it != inputSet.end(); ++it) {
		string temp = *it;
		if (totalLength == 0) {
			// first element ?
			outString << "    " << temp;
			totalLength = temp.length();
		} else {
			outString << ", ";
			if ((totalLength + temp.length()) > columns) {
				outString << "\n    " << temp;
				totalLength = temp.length();
			} else {
				outString << temp;
				totalLength += 2 + temp.length();
			}
		}
	}
	if (totalLength < columns) {
		outString << string(columns - totalLength, ' ');
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

HelpOption::HelpOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool HelpOption::parseOption(const string& /*option*/,
                             deque<string>& /*cmdLine*/)
{
	if (!parser.haveSettings) {
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
	for (map<string, CommandLineParser::OptionData>::const_iterator it =
	        parser.optionMap.begin(); it != parser.optionMap.end(); ++it) {
		optionMap[it->second.option->optionHelp()].insert(it->first);
	}
	printItemMap(optionMap);

	cout << endl;
	cout << "  this is the list of supported file types:" << endl;

	map<string, set<string> > extMap;
	for (CommandLineParser::FileTypeMap::const_iterator it =
	         parser.fileTypeMap.begin();
	     it != parser.fileTypeMap.end(); ++it) {
		extMap[it->second->fileTypeHelp()].insert(it->first);
	}
	printItemMap(extMap);

	parser.parseStatus = CommandLineParser::EXIT;
	return true;
}

const string& HelpOption::optionHelp() const
{
	static const string text("Shows this text");
	return text;
}


// class VersionOption

VersionOption::VersionOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool VersionOption::parseOption(const string& /*option*/,
                                deque<string>& /*cmdLine*/)
{
	cout << Version::FULL_VERSION << endl;
	cout << "flavour: " << BUILD_FLAVOUR << endl;
	cout << "components: " << BUILD_COMPONENTS << endl;
	parser.parseStatus = CommandLineParser::EXIT;
	return true;
}

const string& VersionOption::optionHelp() const
{
	static const string text("Prints openMSX version and exits");
	return text;
}


// Machine option

MachineOption::MachineOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool MachineOption::parseOption(const string& option, deque<string>& cmdLine)
{
	if (parser.haveConfig) {
		throw FatalError("Only one machine option allowed");
	}
	string machine(getArgument(option, cmdLine));
	try {
		parser.reactor.switchMachine(machine);
	} catch (MSXException& e) {
		throw FatalError(e.getMessage());
	}
	parser.haveConfig = true;
	return true;
}
const string& MachineOption::optionHelp() const
{
	static const string text("Use machine specified in argument");
	return text;
}


// Setting Option

SettingOption::SettingOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool SettingOption::parseOption(const string& option, deque<string>& cmdLine)
{
	if (parser.haveSettings) {
		throw FatalError("Only one setting option allowed");
	}
	try {
		CurrentDirFileContext context;
		parser.settingsConfig.loadSetting(
			context, getArgument(option, cmdLine));
		parser.haveSettings = true;
	} catch (FileException& e) {
		throw FatalError(e.getMessage());
	} catch (ConfigException& e) {
		throw FatalError(e.getMessage());
	}
	return true;
}

const string& SettingOption::optionHelp() const
{
	static const string text("Load an alternative settings file");
	return text;
}


// class NoMMXOption

bool NoMMXOption::parseOption(const string& /*option*/,
                              deque<string>& /*cmdLine*/)
{
	cout << "Disabling MMX" << endl;
	HostCPU::getInstance().forceDisableMMX();
	return true;
}

const string& NoMMXOption::optionHelp() const
{
	static const string text(
		"Disables usage of MMX, SSE and SSE2 (for debugging)");
	return text;
}


// class NoSSEOption

bool NoSSEOption::parseOption(const string& /*option*/,
                              deque<string>& /*cmdLine*/)
{
	cout << "Disabling SSE" << endl;
	HostCPU::getInstance().forceDisableSSE();
	return true;
}

const string& NoSSEOption::optionHelp() const
{
	static const string text(
		"Disables usage of SSE and SSE2 (for debugging)");
	return text;
}


// class NoSSE2Option

bool NoSSE2Option::parseOption(const string& /*option*/,
                               deque<string>& /*cmdLine*/)
{
	cout << "Disabling SSE2" << endl;
	HostCPU::getInstance().forceDisableSSE2();
	return true;
}

const string& NoSSE2Option::optionHelp() const
{
	static const string text(
		"Disables usage of SSE2 (for debugging)");
	return text;
}


// class NoPBOOption

bool NoPBOOption::parseOption(const string& /*option*/,
                              deque<string>& /*cmdLine*/)
{
	#if COMPONENT_GL
	cout << "Disabling PBO" << endl;
	PixelBuffers::enabled = false;
	#endif
	return true;
}

const string& NoPBOOption::optionHelp() const
{
	static const string text(
		"Disables usage of openGL PBO (for debugging)");
	return text;
}


// class TestConfigOption

TestConfigOption::TestConfigOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

bool TestConfigOption::parseOption(const string& /*option*/,
                                   deque<string>& /*cmdLine*/)
{
	parser.parseStatus = CommandLineParser::TEST;
	return true;
}

const string& TestConfigOption::optionHelp() const
{
	static const string text("Test if the specified config works and exit");
	return text;
}

} // namespace openmsx
