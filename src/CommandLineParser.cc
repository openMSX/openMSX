#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "Interpreter.hh"
#include "SettingsConfig.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "GlobalCliComm.hh"
#include "StdioMessages.hh"
#include "Version.hh"
#include "CliConnection.hh"
#include "ConfigException.hh"
#include "FileException.hh"
#include "EnumSetting.hh"
#include "XMLException.hh"
#include "StringOp.hh"
#include "xrange.hh"
#include "Reactor.hh"
#include "RomInfo.hh"
#include "hash_map.hh"
#include "one_of.hh"
#include "outer.hh"
#include "ranges.hh"
#include "stl.hh"
#include "view.hh"
#include "xxhash.hh"
#include "build-info.hh"
#include <cassert>
#include <iostream>
#include <memory>

using std::cout;
using std::string;
using std::string_view;

namespace openmsx {

// class CommandLineParser

CommandLineParser::CommandLineParser(Reactor& reactor_)
	: reactor(reactor_)
	, msxRomCLI(*this)
	, cliExtension(*this)
	, replayCLI(*this)
	, saveStateCLI(*this)
	, cassettePlayerCLI(*this)
#if COMPONENT_LASERDISC
	, laserdiscPlayerCLI(*this)
#endif
	, diskImageCLI(*this)
	, hdImageCLI(*this)
	, cdImageCLI(*this)
	, parseStatus(UNPARSED)
{
	haveConfig = false;
	haveSettings = false;

	registerOption("-h",          helpOption,    PHASE_BEFORE_INIT, 1);
	registerOption("--help",      helpOption,    PHASE_BEFORE_INIT, 1);
	registerOption("-v",          versionOption, PHASE_BEFORE_INIT, 1);
	registerOption("--version",   versionOption, PHASE_BEFORE_INIT, 1);
	registerOption("-bash",       bashOption,    PHASE_BEFORE_INIT, 1);

	registerOption("-setting",    settingOption, PHASE_BEFORE_SETTINGS);
	registerOption("-control",    controlOption, PHASE_BEFORE_SETTINGS, 1);
	registerOption("-script",     scriptOption,  PHASE_BEFORE_SETTINGS, 1); // correct phase?
	registerOption("-command",    commandOption, PHASE_BEFORE_SETTINGS, 1); // same phase as -script
	registerOption("-testconfig", testConfigOption, PHASE_BEFORE_SETTINGS, 1);

	registerOption("-machine",    machineOption, PHASE_LOAD_MACHINE);

	registerFileType({"tcl"}, scriptOption);

	// At this point all options and file-types must be registered
	ranges::sort(options, {}, &OptionData::name);
	ranges::sort(fileTypes, StringOp::caseless{}, &FileTypeData::extension);
}

void CommandLineParser::registerOption(
	const char* str, CLIOption& cliOption, ParsePhase phase, unsigned length)
{
	options.emplace_back(OptionData{str, &cliOption, phase, length});
}

void CommandLineParser::registerFileType(
	std::initializer_list<string_view> extensions, CLIFileType& cliFileType)
{
	append(fileTypes, view::transform(extensions,
		[&](auto& ext) { return FileTypeData{ext, &cliFileType}; }));
}

bool CommandLineParser::parseOption(
	const string& arg, span<string>& cmdLine, ParsePhase phase)
{
	if (auto it = ranges::lower_bound(options, arg, {}, &OptionData::name);
	    (it != end(options)) && (it->name == arg)) {
		// parse option
		if (it->phase <= phase) {
			try {
				it->option->parseOption(arg, cmdLine);
				return true;
			} catch (MSXException& e) {
				throw FatalError(std::move(e).getMessage());
			}
		}
	}
	return false; // unknown
}

bool CommandLineParser::parseFileName(const string& arg, span<string>& cmdLine)
{
	if (auto* handler = getFileTypeHandlerForFileName(arg)) {
		try {
			// parse filetype
			handler->parseFileType(arg, cmdLine);
			return true; // file processed
		} catch (MSXException& e) {
			throw FatalError(std::move(e).getMessage());
		}
	}
	return false;
}

CLIFileType* CommandLineParser::getFileTypeHandlerForFileName(string_view filename) const
{
	auto inner = [&](string_view arg) -> CLIFileType* {
		string_view extension = FileOperations::getExtension(arg); // includes leading '.' (if any)
		if (extension.size() <= 1) {
			return nullptr; // no extension -> no handler
		}
		extension.remove_prefix(1);

		auto it = ranges::lower_bound(fileTypes, extension, StringOp::caseless{},
		                              &FileTypeData::extension);
		StringOp::casecmp cmp;
		if ((it == end(fileTypes)) || !cmp(it->extension, extension)) {
			return nullptr; // unknown extension
		}
		return it->fileType;
	};

	// First try the fileName as we get it from the commandline. This may
	// be more interesting than the original fileName of a (g)zipped file:
	// in case of an OMR file for instance, we want to select on the
	// original extension, and not on the extension inside the (g)zipped
	// file.
	auto* result = inner(filename);
	if (!result) {
		try {
			File file(userFileContext().resolve(filename));
			result = inner(file.getOriginalName());
		} catch (FileException&) {
			// ignore
		}
	}
	return result;
}

void CommandLineParser::parse(int argc, char** argv)
{
	parseStatus = RUN;

	auto cmdLineBuf = to_vector(view::transform(xrange(1, argc), [&](auto i) {
		return FileOperations::getConventionalPath(argv[i]);
	}));
	span<string> cmdLine(cmdLineBuf);
	std::vector<string> backupCmdLine;

	for (ParsePhase phase = PHASE_BEFORE_INIT;
	     (phase <= PHASE_LAST) && (parseStatus != EXIT);
	     phase = static_cast<ParsePhase>(phase + 1)) {
		switch (phase) {
		case PHASE_INIT:
			reactor.init();
			fileTypeCategoryInfo.emplace(
				reactor.getOpenMSXInfoCommand(), *this);
			getInterpreter().init(argv[0]);
			break;
		case PHASE_LOAD_SETTINGS:
			// after -control and -setting has been parsed
			if (parseStatus != CONTROL) {
				// if there already is a XML-StdioConnection, we
				// can't also show plain messages on stdout
				auto& cliComm = reactor.getGlobalCliComm();
				cliComm.addListener(std::make_unique<StdioMessages>());
			}
			if (!haveSettings) {
				auto& settingsConfig =
					reactor.getGlobalCommandController().getSettingsConfig();
				// Load default settings file in case the user
				// didn't specify one.
				auto context = systemFileContext();
				string filename = "settings.xml";
				try {
					settingsConfig.loadSetting(context, filename);
				} catch (XMLException& e) {
					reactor.getCliComm().printWarning(
						"Loading of settings failed: ",
						e.getMessage(), "\n"
						"Reverting to default settings.");
				} catch (FileException&) {
					// settings.xml not found
				} catch (ConfigException& e) {
					throw FatalError("Error in default settings: ",
					                 e.getMessage());
				}
				// Consider an attempt to load the settings good enough.
				haveSettings = true;
				// Even if parsing failed, use this file for saving,
				// this forces overwriting a non-setting file.
				settingsConfig.setSaveFilename(context, filename);
			}
			break;
		case PHASE_DEFAULT_MACHINE: {
			if (!haveConfig) {
				// load default config file in case the user didn't specify one
				const auto& machine =
					reactor.getMachineSetting().getString();
				try {
					reactor.switchMachine(string(machine));
				} catch (MSXException& e) {
					reactor.getCliComm().printInfo(
						"Failed to initialize default machine: ",
						e.getMessage());
					// Default machine is broken; fall back to C-BIOS config.
					const auto& fallbackMachine =
						reactor.getMachineSetting().getRestoreValue().getString();
					reactor.getCliComm().printInfo(
						"Using fallback machine: ", fallbackMachine);
					try {
						reactor.switchMachine(string(fallbackMachine));
					} catch (MSXException& e2) {
						// Fallback machine failed as well; we're out of options.
						throw FatalError(std::move(e2).getMessage());
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
				cmdLine = cmdLine.subspan(1);
				// first try options
				if (!parseOption(arg, cmdLine, phase)) {
					// next try the registered filetypes (xml)
					if ((phase != PHASE_LAST) ||
					    !parseFileName(arg, cmdLine)) {
						// no option or known file
						backupCmdLine.push_back(arg);
						if (auto it = ranges::lower_bound(options, arg, {}, &OptionData::name);
						    (it != end(options)) && (it->name == arg)) {
							for (unsigned i = 0; i < it->length - 1; ++i) {
								if (cmdLine.empty()) break;
								backupCmdLine.push_back(std::move(cmdLine.front()));
								cmdLine = cmdLine.subspan(1);
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
	for (const auto& option : options) {
		option.option->parseDone();
	}
	if (!cmdLine.empty() && (parseStatus != EXIT)) {
		throw FatalError(
			"Error parsing command line: ", cmdLine.front(), "\n"
			"Use \"openmsx -h\" to see a list of available options");
	}
}

bool CommandLineParser::isHiddenStartup() const
{
	return parseStatus == one_of(CONTROL, TEST);
}

CommandLineParser::ParseStatus CommandLineParser::getParseStatus() const
{
	assert(parseStatus != UNPARSED);
	return parseStatus;
}

MSXMotherBoard* CommandLineParser::getMotherBoard() const
{
	return reactor.getMotherBoard();
}

GlobalCommandController& CommandLineParser::getGlobalCommandController() const
{
	return reactor.getGlobalCommandController();
}

Interpreter& CommandLineParser::getInterpreter() const
{
	return reactor.getInterpreter();
}


// Control option

void CommandLineParser::ControlOption::parseOption(
	const string& option, span<string>& cmdLine)
{
	const auto& fullType = getArgument(option, cmdLine);
	auto [type, arguments] = StringOp::splitOnFirst(fullType, ':');

	auto& parser = OUTER(CommandLineParser, controlOption);
	auto& controller  = parser.getGlobalCommandController();
	auto& distributor = parser.reactor.getEventDistributor();
	auto& cliComm     = parser.reactor.getGlobalCliComm();
	std::unique_ptr<CliListener> connection;
	if (type == "stdio") {
		connection = std::make_unique<StdioConnection>(
			controller, distributor);
#ifdef _WIN32
	} else if (type == "pipe") {
		connection = std::make_unique<PipeConnection>(
			controller, distributor, arguments);
#endif
	} else {
		throw FatalError("Unknown control type: '", type, '\'');
	}
	cliComm.addListener(std::move(connection));

	parser.parseStatus = CommandLineParser::CONTROL;
}

string_view CommandLineParser::ControlOption::optionHelp() const
{
	return "Enable external control of openMSX process";
}


// Script option

void CommandLineParser::ScriptOption::parseOption(
	const string& option, span<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

string_view CommandLineParser::ScriptOption::optionHelp() const
{
	return "Run extra startup script";
}

void CommandLineParser::ScriptOption::parseFileType(
	const string& filename, span<std::string>& /*cmdLine*/)
{
	scripts.push_back(filename);
}

string_view CommandLineParser::ScriptOption::fileTypeHelp() const
{
	return "Extra Tcl script to run at startup";
}

string_view CommandLineParser::ScriptOption::fileTypeCategoryName() const
{
	return "script";
}


// Command option
void CommandLineParser::CommandOption::parseOption(
	const std::string& option, span<std::string>& cmdLine)
{
	commands.push_back(getArgument(option, cmdLine));
}

std::string_view CommandLineParser::CommandOption::optionHelp() const
{
	return "Run Tcl command at startup (see also -script)";
}


// Help option

static string formatSet(span<const string_view> inputSet, string::size_type columns)
{
	string outString;
	string::size_type totalLength = 0; // ignore the starting spaces for now
	for (const auto& temp : inputSet) {
		if (totalLength == 0) {
			// first element ?
			strAppend(outString, "    ", temp);
			totalLength = temp.size();
		} else {
			outString += ", ";
			if ((totalLength + temp.size()) > columns) {
				strAppend(outString, "\n    ", temp);
				totalLength = temp.size();
			} else {
				strAppend(outString, temp);
				totalLength += 2 + temp.size();
			}
		}
	}
	if (totalLength < columns) {
		outString.append(columns - totalLength, ' ');
	}
	return outString;
}

static string formatHelptext(string_view helpText,
	                     unsigned maxLength, unsigned indent)
{
	string outText;
	string_view::size_type index = 0;
	while (helpText.substr(index).size() > maxLength) {
		auto pos = helpText.substr(index, maxLength).rfind(' ');
		if (pos == string_view::npos) {
			pos = helpText.substr(maxLength).find(' ');
			if (pos == string_view::npos) {
				pos = helpText.substr(index).size();
			}
		}
		strAppend(outText, helpText.substr(index, index + pos), '\n',
		          spaces(indent));
		index = pos + 1;
	}
	strAppend(outText, helpText.substr(index));
	return outText;
}

// items grouped per common help-text
using GroupedItems = hash_map<string_view, std::vector<string_view>, XXHasher>;
static void printItemMap(const GroupedItems& itemMap)
{
	auto printSet = to_vector(view::transform(itemMap, [](auto& p) {
		return strCat(formatSet(p.second, 15), ' ',
		              formatHelptext(p.first, 50, 20));
	}));
	ranges::sort(printSet);
	for (auto& s : printSet) {
		cout << s << '\n';
	}
}


// class HelpOption

void CommandLineParser::HelpOption::parseOption(
	const string& /*option*/, span<string>& /*cmdLine*/)
{
	auto& parser = OUTER(CommandLineParser, helpOption);
	const auto& fullVersion = Version::full();
	cout << fullVersion << '\n'
	     << string(fullVersion.size(), '=') << "\n"
	        "\n"
	        "usage: openmsx [arguments]\n"
	        "  an argument is either an option or a filename\n"
	        "\n"
	        "  this is the list of supported options:\n";

	GroupedItems itemMap;
	for (const auto& option : parser.options) {
		const auto& helpText = option.option->optionHelp();
		if (!helpText.empty()) {
			itemMap[helpText].push_back(option.name);
		}
	}
	printItemMap(itemMap);

	cout << "\n"
	        "  this is the list of supported file types:\n";

	itemMap.clear();
	for (const auto& [extension, fileType] : parser.fileTypes) {
		itemMap[fileType->fileTypeHelp()].push_back(extension);
	}
	printItemMap(itemMap);

	parser.parseStatus = CommandLineParser::EXIT;
}

string_view CommandLineParser::HelpOption::optionHelp() const
{
	return "Shows this text";
}


// class VersionOption

void CommandLineParser::VersionOption::parseOption(
	const string& /*option*/, span<string>& /*cmdLine*/)
{
	cout << Version::full() << "\n"
	        "flavour: " << BUILD_FLAVOUR << "\n"
	        "components: " << BUILD_COMPONENTS << '\n';
	auto& parser = OUTER(CommandLineParser, versionOption);
	parser.parseStatus = CommandLineParser::EXIT;
}

string_view CommandLineParser::VersionOption::optionHelp() const
{
	return "Prints openMSX version and exits";
}


// Machine option

void CommandLineParser::MachineOption::parseOption(
	const string& option, span<string>& cmdLine)
{
	auto& parser = OUTER(CommandLineParser, machineOption);
	if (parser.haveConfig) {
		throw FatalError("Only one machine option allowed");
	}
	try {
		parser.reactor.switchMachine(getArgument(option, cmdLine));
	} catch (MSXException& e) {
		throw FatalError(std::move(e).getMessage());
	}
	parser.haveConfig = true;
}

string_view CommandLineParser::MachineOption::optionHelp() const
{
	return "Use machine specified in argument";
}


// class SettingOption

void CommandLineParser::SettingOption::parseOption(
	const string& option, span<string>& cmdLine)
{
	auto& parser = OUTER(CommandLineParser, settingOption);
	if (parser.haveSettings) {
		throw FatalError("Only one setting option allowed");
	}
	try {
		auto& settingsConfig = parser.reactor.getGlobalCommandController().getSettingsConfig();
		settingsConfig.loadSetting(
			currentDirFileContext(), getArgument(option, cmdLine));
		parser.haveSettings = true;
	} catch (FileException& e) {
		throw FatalError(std::move(e).getMessage());
	} catch (ConfigException& e) {
		throw FatalError(std::move(e).getMessage());
	}
}

string_view CommandLineParser::SettingOption::optionHelp() const
{
	return "Load an alternative settings file";
}


// class TestConfigOption

void CommandLineParser::TestConfigOption::parseOption(
	const string& /*option*/, span<string>& /*cmdLine*/)
{
	auto& parser = OUTER(CommandLineParser, testConfigOption);
	parser.parseStatus = CommandLineParser::TEST;
}

string_view CommandLineParser::TestConfigOption::optionHelp() const
{
	return "Test if the specified config works and exit";
}

// class BashOption

void CommandLineParser::BashOption::parseOption(
	const string& /*option*/, span<string>& cmdLine)
{
	auto& parser = OUTER(CommandLineParser, bashOption);
	string_view last = cmdLine.empty() ? string_view{} : cmdLine.front();
	cmdLine = cmdLine.subspan(0, 0); // eat all remaining parameters

	if (last == "-machine") {
		for (auto& s : Reactor::getHwConfigs("machines")) {
			cout << s << '\n';
		}
	} else if (StringOp::startsWith(last, "-ext")) {
		for (auto& s : Reactor::getHwConfigs("extensions")) {
			cout << s << '\n';
		}
	} else if (last == "-romtype") {
		for (const auto& s : RomInfo::getAllRomTypes()) {
			cout << s << '\n';
		}
	} else {
		for (const auto& option : parser.options) {
			cout << option.name << '\n';
		}
	}
	parser.parseStatus = CommandLineParser::EXIT;
}

string_view CommandLineParser::BashOption::optionHelp() const
{
	return {}; // don't include this option in --help
}

// class FileTypeCategoryInfoTopic

CommandLineParser::FileTypeCategoryInfoTopic::FileTypeCategoryInfoTopic(
		InfoCommand& openMSXInfoCommand, const CommandLineParser& parser_)
	: InfoTopic(openMSXInfoCommand, "file_type_category")
	, parser(parser_)
{
}

void CommandLineParser::FileTypeCategoryInfoTopic::execute(
	span<const TclObject> tokens, TclObject& result) const
{
	checkNumArgs(tokens, 3, "filename");
	assert(tokens.size() == 3);

	// get the category and add it to result
	std::string_view fileName = tokens[2].getString();
	if (const auto* handler = parser.getFileTypeHandlerForFileName(fileName)) {
		result.addListElement(handler->fileTypeCategoryName());
	} else {
		result.addListElement("unknown");
	}
}

string CommandLineParser::FileTypeCategoryInfoTopic::help(span<const TclObject> /*tokens*/) const
{
	return "Returns the file type category for the given file.";
}

} // namespace openmsx
