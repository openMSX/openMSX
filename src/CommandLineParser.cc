// $Id$

#include "CommandLineParser.hh"
#include "CLIOption.hh"
#include "GlobalCommandController.hh"
#include "Interpreter.hh"
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
#include "ReplayCLI.hh"
#include "SaveStateCLI.hh"
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
#include "RomInfo.hh"
#include "StringMap.hh"
#include "memory.hh"
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
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class VersionOption : public CLIOption
{
public:
	explicit VersionOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class ControlOption : public CLIOption
{
public:
	explicit ControlOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class ScriptOption : public CLIOption, public CLIFileType
{
public:
	const CommandLineParser::Scripts& getScripts() const;
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
	virtual void parseFileType(const std::string& filename,
                                   std::deque<std::string>& cmdLine);
	virtual string_ref fileTypeHelp() const;

private:
	CommandLineParser::Scripts scripts;
};

class MachineOption : public CLIOption
{
public:
	explicit MachineOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class SettingOption : public CLIOption
{
public:
	explicit SettingOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class NoMMXOption : public CLIOption
{
public:
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
};

class NoSSEOption : public CLIOption {
public:
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
};

class NoSSE2Option : public CLIOption {
public:
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
};

class NoPBOOption : public CLIOption {
public:
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
};

class TestConfigOption : public CLIOption
{
public:
	explicit TestConfigOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class BashOption : public CLIOption
{
public:
	explicit BashOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, deque<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};


// class CommandLineParser

CommandLineParser::CommandLineParser(Reactor& reactor_)
	: reactor(reactor_)
	, helpOption(make_unique<HelpOption>(*this))
	, versionOption(make_unique<VersionOption>(*this))
	, controlOption(make_unique<ControlOption>(*this))
	, scriptOption(make_unique<ScriptOption>())
	, machineOption(make_unique<MachineOption>(*this))
	, settingOption(make_unique<SettingOption>(*this))
	, noMMXOption(make_unique<NoMMXOption>())
	, noSSEOption(make_unique<NoSSEOption>())
	, noSSE2Option(make_unique<NoSSE2Option>())
	, noPBOOption(make_unique<NoPBOOption>())
	, testConfigOption(make_unique<TestConfigOption>(*this))
	, bashOption(make_unique<BashOption>(*this))
	, msxRomCLI(make_unique<MSXRomCLI>(*this))
	, cliExtension(make_unique<CliExtension>(*this))
	, replayCLI(make_unique<ReplayCLI>(*this))
	, saveStateCLI(make_unique<SaveStateCLI>(*this))
	, cassettePlayerCLI(make_unique<CassettePlayerCLI>(*this))
#if COMPONENT_LASERDISC
	, laserdiscPlayerCLI(make_unique<LaserdiscPlayerCLI>(*this))
#endif
	, diskImageCLI(make_unique<DiskImageCLI>(*this))
	, hdImageCLI(make_unique<HDImageCLI>(*this))
	, cdImageCLI(make_unique<CDImageCLI>(*this))
	, parseStatus(UNPARSED)
{
	haveConfig = false;
	haveSettings = false;

	registerOption("-h",          *helpOption,    PHASE_BEFORE_INIT, 1);
	registerOption("--help",      *helpOption,    PHASE_BEFORE_INIT, 1);
	registerOption("-v",          *versionOption, PHASE_BEFORE_INIT, 1);
	registerOption("--version",   *versionOption, PHASE_BEFORE_INIT, 1);
	registerOption("-bash",       *bashOption,    PHASE_BEFORE_INIT, 1);

	registerOption("-setting",    *settingOption, PHASE_BEFORE_SETTINGS);
	registerOption("-control",    *controlOption, PHASE_BEFORE_SETTINGS, 1);
	registerOption("-script",     *scriptOption,  PHASE_BEFORE_SETTINGS, 1); // correct phase?
	#if ASM_X86
	registerOption("-nommx",      *noMMXOption,   PHASE_BEFORE_SETTINGS, 1);
	registerOption("-nosse",      *noSSEOption,   PHASE_BEFORE_SETTINGS, 1);
	registerOption("-nosse2",     *noSSE2Option,  PHASE_BEFORE_SETTINGS, 1);
	#endif
	#if COMPONENT_GL
	registerOption("-nopbo",      *noPBOOption,   PHASE_BEFORE_SETTINGS, 1);
	#endif
	registerOption("-testconfig", *testConfigOption, PHASE_BEFORE_SETTINGS, 1);

	registerOption("-machine",    *machineOption, PHASE_BEFORE_MACHINE);

	registerFileClass("Tcl script", *scriptOption);
	registerFileTypes();
}

CommandLineParser::~CommandLineParser()
{
}

void CommandLineParser::registerOption(
	const char* str, CLIOption& cliOption, ParsePhase phase, unsigned length)
{
	OptionData temp;
	temp.option = &cliOption;
	temp.phase = phase;
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
	map<string, string> fileExtMap;
	fileExtMap["rom"] = "romimage";
	fileExtMap["ri"]  = "romimage";
	fileExtMap["dsk"] = "diskimage";
	fileExtMap["dmk"] = "diskimage";
	fileExtMap["di1"] = "diskimage";
	fileExtMap["di2"] = "diskimage";
	fileExtMap["xsa"] = "diskimage";
	fileExtMap["wav"] = "cassetteimage";
	fileExtMap["cas"] = "cassetteimage";
	fileExtMap["ogv"] = "laserdiscimage";
	fileExtMap["omr"] = "openMSX replay";
	fileExtMap["oms"] = "openMSX savestate";
	fileExtMap["tcl"] = "Tcl script";
	for (map<string, string>::const_iterator j = fileExtMap.begin();
	     j != fileExtMap.end(); ++j) {
		FileClassMap::const_iterator i = fileClassMap.find(j->second);
		if (i != fileClassMap.end()) {
			fileTypeMap[j->first] = i->second;
		}
	}
}

bool CommandLineParser::parseOption(
	const string& arg, deque<string>& cmdLine, ParsePhase phase)
{
	map<string, OptionData>::const_iterator it1 = optionMap.find(arg);
	if (it1 != optionMap.end()) {
		// parse option
		if (it1->second.phase <= phase) {
			try {
				it1->second.option->parseOption(arg, cmdLine);
				return true;
			} catch (MSXException& e) {
				throw FatalError(e.getMessage());
			}
		}
	}
	return false; // unknown
}

bool CommandLineParser::parseFileName(const string& arg, deque<string>& cmdLine)
{
	// First try the fileName as we get it from the commandline. This may
	// be more interesting than the original fileName of a (g)zipped file:
	// in case of an OMR file for instance, we want to select on the
	// original extension, and not on the extension inside the gzipped
	// file.
	bool processed = parseFileNameInner(arg, arg, cmdLine);
	if (!processed) {
		try {
			UserFileContext context;
			File file(context.resolve(arg));
			string originalName = file.getOriginalName();
			processed = parseFileNameInner(originalName, arg, cmdLine);
		} catch (FileException&) {
			// ignore
		}
	}
	return processed;
}

bool CommandLineParser::parseFileNameInner(const string& name, const string& originalPath, deque<string>& cmdLine)
{
	string_ref extension = FileOperations::getExtension(name);
	if (!extension.empty()) {
		// there is an extension
		FileTypeMap::const_iterator it = fileTypeMap.find(extension.str());
		if (it != fileTypeMap.end()) {
			try {
				// parse filetype
				it->second->parseFileType(originalPath, cmdLine);
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

	for (ParsePhase phase = PHASE_BEFORE_INIT;
	     (phase <= PHASE_LAST) && (parseStatus != EXIT);
	     phase = static_cast<ParsePhase>(phase + 1)) {
		switch (phase) {
		case PHASE_INIT:
			reactor.init();
			reactor.getGlobalCommandController().getInterpreter().init(argv[0]);
			break;
		case PHASE_LOAD_SETTINGS:
			// after -control and -setting has been parsed
			if (parseStatus != CONTROL) {
				// if there already is a XML-StdioConnection, we
				// can't also show plain messages on stdout
				GlobalCliComm& cliComm = reactor.getGlobalCliComm();
				cliComm.addListener(new StdioMessages());
			}
			if (!haveSettings) {
				SettingsConfig& settingsConfig =
					reactor.getGlobalCommandController().getSettingsConfig();
				// Load default settings file in case the user
				// didn't specify one.
				SystemFileContext context;
				string filename = "settings.xml";
				try {
					settingsConfig.loadSetting(context, filename);
				} catch (XMLException& e) {
					reactor.getCliComm().printWarning(
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
		case PHASE_LOAD_MACHINE: {
			if (!haveConfig) {
				// load default config file in case the user didn't specify one
				const string& machine =
					reactor.getMachineSetting().getValueString();
				try {
					reactor.switchMachine(machine);
				} catch (MSXException& e) {
					reactor.getCliComm().printInfo(
						"Failed to initialize default machine: " + e.getMessage());
					// Default machine is broken; fall back to C-BIOS config.
					const string& fallbackMachine =
						reactor.getMachineSetting().getRestoreValueString();
					reactor.getCliComm().printInfo("Using fallback machine: " + fallbackMachine);
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
				if (!parseOption(arg, cmdLine, phase)) {
					// next try the registered filetypes (xml)
					if ((phase != PHASE_LAST) ||
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

void ControlOption::parseOption(const string& option, deque<string>& cmdLine)
{
	string fullType = getArgument(option, cmdLine);
	string_ref type, arguments;
	StringOp::splitOnFirst(fullType, ':', type, arguments);

	CommandController& controller = parser.getGlobalCommandController();
	EventDistributor& distributor = parser.reactor.getEventDistributor();
	GlobalCliComm& cliComm        = parser.reactor.getGlobalCliComm();
	std::unique_ptr<CliListener> connection;
	if (type == "stdio") {
		connection = make_unique<StdioConnection>(
			controller, distributor);
#ifdef _WIN32
	} else if (type == "pipe") {
		OSVERSIONINFO info;
		info.dwOSVersionInfoSize = sizeof(info);
		GetVersionEx(&info);
		if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			connection = make_unique<PipeConnection>(
				controller, distributor, arguments);
		} else {
			throw FatalError("Pipes are not supported on this "
			                 "version of Windows");
		}
#endif
	} else {
		throw FatalError("Unknown control type: '"  + type + '\'');
	}
	cliComm.addListener(connection.release());

	parser.parseStatus = CommandLineParser::CONTROL;
}

string_ref ControlOption::optionHelp() const
{
	return "Enable external control of openMSX process";
}


// Script option

const CommandLineParser::Scripts& ScriptOption::getScripts() const
{
	return scripts;
}

void ScriptOption::parseOption(const string& option, deque<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

string_ref ScriptOption::optionHelp() const
{
	return "Run extra startup script";
}

void ScriptOption::parseFileType(const string& filename,
                                   deque<std::string>& /*cmdLine*/)
{
	scripts.push_back(filename);
}

string_ref ScriptOption::fileTypeHelp() const
{
	return "Extra Tcl script to run at startup";
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

static string formatHelptext(string_ref helpText,
	                     unsigned maxLength, unsigned indent)
{
	string outText;
	string_ref::size_type index = 0;
	while (helpText.substr(index).size() > maxLength) {
		string_ref::size_type pos = helpText.substr(index, maxLength).rfind(' ');
		if (pos == string_ref::npos) {
			pos = helpText.substr(maxLength).find(' ');
			if (pos == string_ref::npos) {
				pos = helpText.substr(index).size();
			}
		}
		outText += helpText.substr(index, index + pos) + '\n' +
		           string(indent, ' ');
		index = pos + 1;
	}
	string_ref t = helpText.substr(index);
	outText.append(t.data(), t.size());
	return outText;
}

static void printItemMap(const StringMap<set<string>>& itemMap)
{
	set<string> printSet;
	for (StringMap<set<string>>::const_iterator it = itemMap.begin();
	     it != itemMap.end(); ++it) {
		printSet.insert(formatSet(it->second, 15) + ' ' +
		                formatHelptext(it->first(), 50, 20));
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

void HelpOption::parseOption(const string& /*option*/,
                             deque<string>& /*cmdLine*/)
{
	string fullVersion = Version::full();
	cout << fullVersion << endl;
	cout << string(fullVersion.size(), '=') << endl;
	cout << endl;
	cout << "usage: openmsx [arguments]" << endl;
	cout << "  an argument is either an option or a filename" << endl;
	cout << endl;
	cout << "  this is the list of supported options:" << endl;

	StringMap<set<string>> optionMap;
	for (map<string, CommandLineParser::OptionData>::const_iterator it =
	        parser.optionMap.begin(); it != parser.optionMap.end(); ++it) {
		string_ref helpText = it->second.option->optionHelp();
		if (!helpText.empty()) {
			optionMap[helpText].insert(it->first);
		}
	}
	printItemMap(optionMap);

	cout << endl;
	cout << "  this is the list of supported file types:" << endl;

	StringMap<set<string>> extMap;
	for (CommandLineParser::FileTypeMap::const_iterator it =
	         parser.fileTypeMap.begin();
	     it != parser.fileTypeMap.end(); ++it) {
		extMap[it->second->fileTypeHelp()].insert(it->first);
	}
	printItemMap(extMap);

	parser.parseStatus = CommandLineParser::EXIT;
}

string_ref HelpOption::optionHelp() const
{
	return "Shows this text";
}


// class VersionOption

VersionOption::VersionOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

void VersionOption::parseOption(const string& /*option*/,
                                deque<string>& /*cmdLine*/)
{
	cout << Version::full() << endl;
	cout << "flavour: " << BUILD_FLAVOUR << endl;
	cout << "components: " << BUILD_COMPONENTS << endl;
	parser.parseStatus = CommandLineParser::EXIT;
}

string_ref VersionOption::optionHelp() const
{
	return "Prints openMSX version and exits";
}


// Machine option

MachineOption::MachineOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

void MachineOption::parseOption(const string& option, deque<string>& cmdLine)
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
}
string_ref MachineOption::optionHelp() const
{
	return "Use machine specified in argument";
}


// Setting Option

SettingOption::SettingOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

void SettingOption::parseOption(const string& option, deque<string>& cmdLine)
{
	if (parser.haveSettings) {
		throw FatalError("Only one setting option allowed");
	}
	try {
		SettingsConfig& settingsConfig = parser.reactor.getGlobalCommandController().getSettingsConfig();
		CurrentDirFileContext context;
		settingsConfig.loadSetting(
			context, getArgument(option, cmdLine));
		parser.haveSettings = true;
	} catch (FileException& e) {
		throw FatalError(e.getMessage());
	} catch (ConfigException& e) {
		throw FatalError(e.getMessage());
	}
}

string_ref SettingOption::optionHelp() const
{
	return "Load an alternative settings file";
}


// class NoMMXOption

void NoMMXOption::parseOption(const string& /*option*/,
                              deque<string>& /*cmdLine*/)
{
	cout << "Disabling MMX" << endl;
	HostCPU::forceDisableMMX();
}

string_ref NoMMXOption::optionHelp() const
{
	return "Disables usage of MMX, SSE and SSE2 (for debugging)";
}


// class NoSSEOption

void NoSSEOption::parseOption(const string& /*option*/,
                              deque<string>& /*cmdLine*/)
{
	cout << "Disabling SSE" << endl;
	HostCPU::forceDisableSSE();
}

string_ref NoSSEOption::optionHelp() const
{
	return "Disables usage of SSE and SSE2 (for debugging)";
}


// class NoSSE2Option

void NoSSE2Option::parseOption(const string& /*option*/,
                               deque<string>& /*cmdLine*/)
{
	cout << "Disabling SSE2" << endl;
	HostCPU::forceDisableSSE2();
}

string_ref NoSSE2Option::optionHelp() const
{
	return "Disables usage of SSE2 (for debugging)";
}


// class NoPBOOption

void NoPBOOption::parseOption(const string& /*option*/,
                              deque<string>& /*cmdLine*/)
{
	#if COMPONENT_GL
	cout << "Disabling PBO" << endl;
	PixelBuffers::enabled = false;
	#endif
}

string_ref NoPBOOption::optionHelp() const
{
	return "Disables usage of openGL PBO (for debugging)";
}


// class TestConfigOption

TestConfigOption::TestConfigOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

void TestConfigOption::parseOption(const string& /*option*/,
                                   deque<string>& /*cmdLine*/)
{
	parser.parseStatus = CommandLineParser::TEST;
}

string_ref TestConfigOption::optionHelp() const
{
	return "Test if the specified config works and exit";
}

// class BashOption

BashOption::BashOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

void BashOption::parseOption(const string& /*option*/,
                             deque<string>& cmdLine)
{
	string last = cmdLine.empty() ? "" : cmdLine.front();
	cmdLine.clear(); // eat all remaining parameters

	set<string> items;
	if (last == "-machine") {
		Reactor::getHwConfigs("machines", items);
	} else if (last == "-ext") {
		Reactor::getHwConfigs("extensions", items);
	} else if (last == "-romtype") {
		RomInfo::getAllRomTypes(items);
	} else {
		for (map<string, CommandLineParser::OptionData>::const_iterator it =
			parser.optionMap.begin(); it != parser.optionMap.end(); ++it) {
			items.insert(it->first);
		}
	}
	for (set<string>::const_iterator it = items.begin();
	     it != items.end(); ++it) {
		cout << *it << '\n';
	}
	parser.parseStatus = CommandLineParser::EXIT;
}

string_ref BashOption::optionHelp() const
{
	return ""; // don't include this option in --help
}

} // namespace openmsx
