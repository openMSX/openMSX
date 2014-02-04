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
#include "XMLException.hh"
#include "StringOp.hh"
#include "xrange.hh"
#include "GLUtil.hh"
#include "Reactor.hh"
#include "RomInfo.hh"
#include "StringMap.hh"
#include "memory.hh"
#include "stl.hh"
#include "build-info.hh"
#include "components.hh"

#if COMPONENT_LASERDISC
#include "LaserdiscPlayerCLI.hh"
#endif

#include <cassert>
#include <iostream>

using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace openmsx {

class HelpOption : public CLIOption
{
public:
	explicit HelpOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, array_ref<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class VersionOption : public CLIOption
{
public:
	explicit VersionOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, array_ref<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class ControlOption : public CLIOption
{
public:
	explicit ControlOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, array_ref<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class ScriptOption : public CLIOption, public CLIFileType
{
public:
	const CommandLineParser::Scripts& getScripts() const;
	virtual void parseOption(const string& option, array_ref<string>& cmdLine);
	virtual string_ref optionHelp() const;
	virtual void parseFileType(const std::string& filename,
                                   array_ref<std::string>& cmdLine);
	virtual string_ref fileTypeHelp() const;

private:
	CommandLineParser::Scripts scripts;
};

class MachineOption : public CLIOption
{
public:
	explicit MachineOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, array_ref<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class SettingOption : public CLIOption
{
public:
	explicit SettingOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, array_ref<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class NoPBOOption : public CLIOption {
public:
	virtual void parseOption(const string& option, array_ref<string>& cmdLine);
	virtual string_ref optionHelp() const;
};

class TestConfigOption : public CLIOption
{
public:
	explicit TestConfigOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, array_ref<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};

class BashOption : public CLIOption
{
public:
	explicit BashOption(CommandLineParser& parser);
	virtual void parseOption(const string& option, array_ref<string>& cmdLine);
	virtual string_ref optionHelp() const;
private:
	CommandLineParser& parser;
};


// class CommandLineParser

typedef LessTupleElement<0> CmpOptions;
typedef CmpTupleElement<0, StringOp::caseless> CmpFileTypes;

CommandLineParser::CommandLineParser(Reactor& reactor_)
	: reactor(reactor_)
	, helpOption(make_unique<HelpOption>(*this))
	, versionOption(make_unique<VersionOption>(*this))
	, controlOption(make_unique<ControlOption>(*this))
	, scriptOption(make_unique<ScriptOption>())
	, machineOption(make_unique<MachineOption>(*this))
	, settingOption(make_unique<SettingOption>(*this))
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
	registerOption("-script",     *scriptOption,  PHASE_BEFORE_SETTINGS, 1); // correct phase?
	#if COMPONENT_GL
	registerOption("-nopbo",      *noPBOOption,   PHASE_BEFORE_SETTINGS, 1);
	#endif
	registerOption("-testconfig", *testConfigOption, PHASE_BEFORE_SETTINGS, 1);

	registerOption("-machine",    *machineOption, PHASE_BEFORE_MACHINE);

	registerOption("-control",    *controlOption, PHASE_LAST, 1);

	registerFileType("tcl", *scriptOption);

	// At this point all options and file-types must be registered
	sort(options.begin(), options.end(), CmpOptions());
	sort(fileTypes.begin(), fileTypes.end(), CmpFileTypes());
}

CommandLineParser::~CommandLineParser()
{
}

void CommandLineParser::registerOption(
	const char* str, CLIOption& cliOption, ParsePhase phase, unsigned length)
{
	options.emplace_back(str, OptionData{&cliOption, phase, length});
}

void CommandLineParser::registerFileType(
	string_ref extensions, CLIFileType& cliFileType)
{
	for (auto& ext: StringOp::split(extensions, ',')) {
		fileTypes.emplace_back(ext, &cliFileType);
	}
}

bool CommandLineParser::parseOption(
	const string& arg, array_ref<string>& cmdLine, ParsePhase phase)
{
	auto it = lower_bound(options.begin(), options.end(), arg,
	                      CmpOptions());
	if ((it != options.end()) && (it->first == arg)) {
		// parse option
		if (it->second.phase <= phase) {
			try {
				it->second.option->parseOption(arg, cmdLine);
				return true;
			} catch (MSXException& e) {
				throw FatalError(e.getMessage());
			}
		}
	}
	return false; // unknown
}

bool CommandLineParser::parseFileName(const string& arg, array_ref<string>& cmdLine)
{
	// First try the fileName as we get it from the commandline. This may
	// be more interesting than the original fileName of a (g)zipped file:
	// in case of an OMR file for instance, we want to select on the
	// original extension, and not on the extension inside the gzipped
	// file.
	bool processed = parseFileNameInner(arg, arg, cmdLine);
	if (!processed) {
		try {
			File file(UserFileContext().resolve(arg));
			string originalName = file.getOriginalName();
			processed = parseFileNameInner(originalName, arg, cmdLine);
		} catch (FileException&) {
			// ignore
		}
	}
	return processed;
}

bool CommandLineParser::parseFileNameInner(const string& name, const string& originalPath, array_ref<string>& cmdLine)
{
	string_ref extension = FileOperations::getExtension(name);
	if (extension.empty()) {
		return false; // no extension
	}

	auto it = lower_bound(fileTypes.begin(), fileTypes.end(), extension,
	                      CmpFileTypes());
	StringOp::casecmp cmp;
	if ((it == fileTypes.end()) || !cmp(it->first, extension)) {
		return false; // unknown extension
	}

	try {
		// parse filetype
		it->second->parseFileType(originalPath, cmdLine);
		return true; // file processed
	} catch (MSXException& e) {
		throw FatalError(e.getMessage());
	}
}

void CommandLineParser::parse(int argc, char** argv)
{
	parseStatus = RUN;

	vector<string> cmdLineBuf;
	for (auto i : xrange(1, argc)) {
		cmdLineBuf.push_back(FileOperations::getConventionalPath(argv[i]));
	}
	array_ref<string> cmdLine(cmdLineBuf);
	vector<string> backupCmdLine;

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
				auto& cliComm = reactor.getGlobalCliComm();
				cliComm.addListener(new StdioMessages());
			}
			if (!haveSettings) {
				auto& settingsConfig =
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
				const auto& machine =
					reactor.getMachineSetting().getString();
				try {
					reactor.switchMachine(machine);
				} catch (MSXException& e) {
					reactor.getCliComm().printInfo(
						"Failed to initialize default machine: " + e.getMessage());
					// Default machine is broken; fall back to C-BIOS config.
					const auto& fallbackMachine =
						reactor.getMachineSetting().getRestoreValue();
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
				string arg = std::move(cmdLine.front());
				cmdLine.pop_front();
				// first try options
				if (!parseOption(arg, cmdLine, phase)) {
					// next try the registered filetypes (xml)
					if ((phase != PHASE_LAST) ||
					    !parseFileName(arg, cmdLine)) {
						// no option or known file
						backupCmdLine.push_back(arg);
						auto it = lower_bound(options.begin(), options.end(), arg, CmpOptions());
						if ((it != options.end()) && (it->first == arg)) {
							for (unsigned i = 0; i < it->second.length - 1; ++i) {
								if (!cmdLine.empty()) {
									backupCmdLine.push_back(std::move(cmdLine.front()));
									cmdLine.pop_front();
								}
							}
						}
					}
				}
			}
			std::swap(backupCmdLine, cmdLineBuf);
			backupCmdLine.clear();
			cmdLine = cmdLineBuf;
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

void ControlOption::parseOption(const string& option, array_ref<string>& cmdLine)
{
	const auto& fullType = getArgument(option, cmdLine);
	string_ref type, arguments;
	StringOp::splitOnFirst(fullType, ':', type, arguments);

	auto& controller  = parser.getGlobalCommandController();
	auto& distributor = parser.reactor.getEventDistributor();
	auto& cliComm     = parser.reactor.getGlobalCliComm();
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

void ScriptOption::parseOption(const string& option, array_ref<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

string_ref ScriptOption::optionHelp() const
{
	return "Run extra startup script";
}

void ScriptOption::parseFileType(const string& filename,
                                 array_ref<std::string>& /*cmdLine*/)
{
	scripts.push_back(filename);
}

string_ref ScriptOption::fileTypeHelp() const
{
	return "Extra Tcl script to run at startup";
}

// Help option

static string formatSet(const vector<string_ref>& inputSet, string::size_type columns)
{
	StringOp::Builder outString;
	string::size_type totalLength = 0; // ignore the starting spaces for now
	for (auto& temp : inputSet) {
		if (totalLength == 0) {
			// first element ?
			outString << "    " << temp;
			totalLength = temp.size();
		} else {
			outString << ", ";
			if ((totalLength + temp.size()) > columns) {
				outString << "\n    " << temp;
				totalLength = temp.size();
			} else {
				outString << temp;
				totalLength += 2 + temp.size();
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
		auto pos = helpText.substr(index, maxLength).rfind(' ');
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

static void printItemMap(const StringMap<vector<string_ref>>& itemMap)
{
	vector<string> printSet;
	for (auto& p : itemMap) {
		printSet.push_back(formatSet(p.second, 15) + ' ' +
		                   formatHelptext(p.first(), 50, 20));
	}
	sort(printSet.begin(), printSet.end());
	for (auto& s : printSet) {
		cout << s << endl;
	}
}

HelpOption::HelpOption(CommandLineParser& parser_)
	: parser(parser_)
{
}

void HelpOption::parseOption(const string& /*option*/,
                             array_ref<string>& /*cmdLine*/)
{
	const auto& fullVersion = Version::full();
	cout << fullVersion << endl;
	cout << string(fullVersion.size(), '=') << endl;
	cout << endl;
	cout << "usage: openmsx [arguments]" << endl;
	cout << "  an argument is either an option or a filename" << endl;
	cout << endl;
	cout << "  this is the list of supported options:" << endl;

	StringMap<vector<string_ref>> optionMap;
	for (auto& p : parser.options) {
		const auto& helpText = p.second.option->optionHelp();
		if (!helpText.empty()) {
			optionMap[helpText].push_back(p.first);
		}
	}
	printItemMap(optionMap);

	cout << endl;
	cout << "  this is the list of supported file types:" << endl;

	StringMap<vector<string_ref>> extMap;
	for (auto& p : parser.fileTypes) {
		extMap[p.second->fileTypeHelp()].push_back(p.first);
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
                                array_ref<string>& /*cmdLine*/)
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

void MachineOption::parseOption(const string& option, array_ref<string>& cmdLine)
{
	if (parser.haveConfig) {
		throw FatalError("Only one machine option allowed");
	}
	try {
		parser.reactor.switchMachine(getArgument(option, cmdLine));
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

void SettingOption::parseOption(const string& option, array_ref<string>& cmdLine)
{
	if (parser.haveSettings) {
		throw FatalError("Only one setting option allowed");
	}
	try {
		auto& settingsConfig = parser.reactor.getGlobalCommandController().getSettingsConfig();
		settingsConfig.loadSetting(
			CurrentDirFileContext(), getArgument(option, cmdLine));
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


// class NoPBOOption

void NoPBOOption::parseOption(const string& /*option*/,
                              array_ref<string>& /*cmdLine*/)
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
                                   array_ref<string>& /*cmdLine*/)
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
                             array_ref<string>& cmdLine)
{
	string last = cmdLine.empty() ? "" : cmdLine.front();
	cmdLine.clear(); // eat all remaining parameters

	if (last == "-machine") {
		for (auto& s : Reactor::getHwConfigs("machines")) {
			cout << s << '\n';
		}
	} else if (last == "-ext") {
		for (auto& s : Reactor::getHwConfigs("extensions")) {
			cout << s << '\n';
		}
	} else if (last == "-romtype") {
		for (auto& s : RomInfo::getAllRomTypes()) {
			cout << s << '\n';
		}
	} else {
		for (auto& p : parser.options) {
			cout << p.first << '\n';
		}
	}
	parser.parseStatus = CommandLineParser::EXIT;
}

string_ref BashOption::optionHelp() const
{
	return ""; // don't include this option in --help
}

} // namespace openmsx
