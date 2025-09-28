#include "CommandLineParser.hh"

#include "CliConnection.hh"
#include "ConfigException.hh"
#include "EnumSetting.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileOperations.hh"
#include "GlobalCliComm.hh"
#include "GlobalCommandController.hh"
#include "Interpreter.hh"
#include "Reactor.hh"
#include "RomInfo.hh"
#include "SettingsConfig.hh"
#include "StdioMessages.hh"
#include "Version.hh"
#include "XMLException.hh"

#include "StringOp.hh"
#include "foreach_file.hh"
#include "hash_map.hh"
#include "outer.hh"
#include "ranges.hh"
#include "stl.hh"
#include "xxhash.hh"

#include "build-info.hh"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <ranges>

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
{
	using enum Phase;
	registerOption("-h",          helpOption,    BEFORE_INIT, 1);
	registerOption("--help",      helpOption,    BEFORE_INIT, 1);
	registerOption("-v",          versionOption, BEFORE_INIT, 1);
	registerOption("--version",   versionOption, BEFORE_INIT, 1);
	registerOption("-bash",       bashOption,    BEFORE_INIT, 1);

	registerOption("-setting",    settingOption, BEFORE_SETTINGS);
	registerOption("-control",    controlOption, BEFORE_SETTINGS, 1);
	registerOption("-script",     scriptOption,  BEFORE_SETTINGS, 1); // correct phase?
	registerOption("-command",    commandOption, BEFORE_SETTINGS, 1); // same phase as -script
	registerOption("-testconfig", testConfigOption, BEFORE_SETTINGS, 1);

	registerOption("-machine",    machineOption, LOAD_MACHINE);
	registerOption("-setup",      setupOption,   LOAD_MACHINE);

	registerFileType(std::array<std::string_view, 1>{"tcl"}, scriptOption);

	// At this point all options and file-types must be registered
	std::ranges::sort(options, {}, &OptionData::name);
	std::ranges::sort(fileTypes, StringOp::caseless{}, &FileTypeData::extension);
}

void CommandLineParser::registerOption(
	std::string_view str, CLIOption& cliOption, Phase phase, unsigned length)
{
	options.emplace_back(str, &cliOption, phase, length);
}

void CommandLineParser::registerFileType(
	std::span<const std::string_view> extensions, CLIFileType& cliFileType)
{
	append(fileTypes, std::views::transform(extensions,
		[&](auto& ext) { return FileTypeData{ext, &cliFileType}; }));
}

bool CommandLineParser::parseOption(
	const std::string& arg, std::span<std::string>& cmdLine, Phase phase)
{
	if (auto o = binary_find(options, arg, {}, &OptionData::name)) {
		// parse option
		if (o->phase <= phase) {
			try {
				o->option->parseOption(arg, cmdLine);
				return true;
			} catch (MSXException& e) {
				throw FatalError(std::move(e).getMessage());
			}
		}
	}
	return false; // unknown
}

bool CommandLineParser::parseFileName(const std::string& arg, std::span<std::string>& cmdLine)
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

CLIFileType* CommandLineParser::getFileTypeHandlerForFileName(std::string_view filename) const
{
	auto inner = [&](std::string_view arg) -> CLIFileType* {
		std::string_view extension = FileOperations::getExtension(arg); // includes leading '.' (if any)
		if (extension.size() <= 1) {
			return nullptr; // no extension -> no handler
		}
		extension.remove_prefix(1);

		auto f = binary_find(fileTypes, extension, StringOp::caseless{},
		                     &FileTypeData::extension);
		return f ? f->fileType : nullptr;
	};

	// First try the fileName as we get it from the command line. This may
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

void CommandLineParser::parse(std::span<char*> argv)
{
	parseStatus = Status::RUN;

	auto cmdLineBuf = to_vector(std::views::transform(std::views::drop(argv, 1), [](const char* a) {
		return FileOperations::getConventionalPath(a);
	}));
	std::span<std::string> cmdLine(cmdLineBuf);
	std::vector<std::string> backupCmdLine;

	using enum Phase;
	for (Phase phase = BEFORE_INIT;
	     (phase <= LAST) && (parseStatus != Status::EXIT);
	     phase = static_cast<Phase>(std::to_underlying(phase) + 1)) {
		switch (phase) {
		case INIT:
			reactor.init();
			fileTypeCategoryInfo.emplace(
				reactor.getOpenMSXInfoCommand(), *this);
			getInterpreter().init(argv[0]);
			break;
		case LOAD_SETTINGS:
			// after -control and -setting has been parsed
			if (parseStatus != Status::CONTROL) {
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
				const auto& context = systemFileContext();
				std::string filename = "settings.xml";
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
		case DEFAULT_MACHINE: {
			if (!haveConfig) {
				// load default setup in case the user didn't specify one
				const auto& defaultSetup =
					reactor.getDefaultSetupSetting().getString();
				if (!defaultSetup.empty()) {
					auto context = userDataFileContext(Reactor::SETUP_DIR);
					std::string filename;
					try {
						// Setups are specified without extension
						filename = context.resolve(tmpStrCat(
							defaultSetup, Reactor::SETUP_EXTENSION));
					} catch (MSXException& e) {
						reactor.getCliComm().printInfo(
							"Failed to load default setup: ", e.getMessage());
					}
					if (!filename.empty()) {
						try {
							reactor.switchMachineFromSetup(filename);
							haveConfig = true;
						} catch (MSXException& e) {
							reactor.getCliComm().printInfo(
								"Failed to activate default setup: ",
								e.getMessage());
						}
					}
				}
				if (!haveConfig) {
					// load default machine config file in case the user
					// didn't specify one and loading the default setup failed
					const auto& defaultMachine =
						reactor.getDefaultMachineSetting().getString();
					try {
						reactor.switchMachine(std::string(defaultMachine));
					} catch (MSXException& e) {
						reactor.getCliComm().printInfo(
							"Failed to initialize default machine: ",
							e.getMessage());
						// Default machine is broken; fall back to C-BIOS config.
						auto fallbackMachine = std::string(
							reactor.getDefaultMachineSetting().getDefaultValue().getString());
						reactor.getCliComm().printInfo(
							"Using fallback machine: ", fallbackMachine);
						try {
							reactor.switchMachine(fallbackMachine);
						} catch (MSXException& e2) {
							// Fallback machine failed as well; we're out of options.
							throw FatalError(std::move(e2).getMessage());
						}
					}
				}
				haveConfig = true;
			}
			break;
		}
		default:
			// iterate over all arguments
			while (!cmdLine.empty()) {
				std::string arg = std::move(cmdLine.front());
				cmdLine = cmdLine.subspan(1);
				// first try options
				if (!parseOption(arg, cmdLine, phase)) {
					// next try the registered filetypes (xml)
					if ((phase != LAST) ||
					    !parseFileName(arg, cmdLine)) {
						// no option or known file
						backupCmdLine.push_back(arg);
						if (auto o = binary_find(options, arg, {}, &OptionData::name)) {
							for (unsigned i = 0; i < o->length - 1; ++i) {
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
	if (!cmdLine.empty() && (parseStatus != Status::EXIT)) {
		throw FatalError(
			"Error parsing command line: ", cmdLine.front(), "\n"
			"Use \"openmsx -h\" to see a list of available options");
	}
}

CommandLineParser::Status CommandLineParser::getParseStatus() const
{
	assert(parseStatus != Status::UNPARSED);
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
	const std::string& option, std::span<std::string>& cmdLine)
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

	parser.parseStatus = CommandLineParser::Status::CONTROL;
}

std::string_view CommandLineParser::ControlOption::optionHelp() const
{
	return "Enable external control of openMSX process";
}


// Script option

void CommandLineParser::ScriptOption::parseOption(
	const std::string& option, std::span<std::string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

std::string_view CommandLineParser::ScriptOption::optionHelp() const
{
	return "Run extra startup script";
}

void CommandLineParser::ScriptOption::parseFileType(
	const std::string& filename, std::span<std::string>& /*cmdLine*/)
{
	scripts.push_back(filename);
}

std::string_view CommandLineParser::ScriptOption::fileTypeHelp() const
{
	return "Extra Tcl script to run at startup";
}

std::string_view CommandLineParser::ScriptOption::fileTypeCategoryName() const
{
	return "script";
}


// Command option
void CommandLineParser::CommandOption::parseOption(
	const std::string& option, std::span<std::string>& cmdLine)
{
	commands.push_back(getArgument(option, cmdLine));
}

std::string_view CommandLineParser::CommandOption::optionHelp() const
{
	return "Run Tcl command at startup (see also -script)";
}


// Help option

static std::string formatSet(std::span<const std::string_view> inputSet, std::string::size_type columns)
{
	std::string outString;
	std::string::size_type totalLength = 0; // ignore the starting spaces for now
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

static std::string formatHelpText(std::string_view helpText,
                                  unsigned maxLength, unsigned indent)
{
	std::string outText;
	std::string_view::size_type index = 0;
	while (helpText.substr(index).size() > maxLength) {
		auto pos = helpText.substr(index, maxLength).rfind(' ');
		if (pos == std::string_view::npos) {
			pos = helpText.substr(maxLength).find(' ');
			if (pos == std::string_view::npos) {
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
using GroupedItems = hash_map<std::string_view, std::vector<std::string_view>, XXHasher>;
static void printItemMap(const GroupedItems& itemMap)
{
	auto printSet = to_vector(std::views::transform(itemMap, [](auto& p) {
		return strCat(formatSet(p.second, 15), ' ',
		              formatHelpText(p.first, 50, 20));
	}));
	std::ranges::sort(printSet);
	for (const auto& s : printSet) {
		std::cout << s << '\n';
	}
}


// class HelpOption

void CommandLineParser::HelpOption::parseOption(
	const std::string& /*option*/, std::span<std::string>& /*cmdLine*/)
{
	auto& parser = OUTER(CommandLineParser, helpOption);
	const auto& fullVersion = Version::full();
	std::cout << fullVersion << '\n'
	          << std::string(fullVersion.size(), '=') << "\n"
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

	std::cout << "\n"
	             "  this is the list of supported file types:\n";

	itemMap.clear();
	for (const auto& [extension, fileType] : parser.fileTypes) {
		itemMap[fileType->fileTypeHelp()].push_back(extension);
	}
	printItemMap(itemMap);

	parser.parseStatus = CommandLineParser::Status::EXIT;
}

std::string_view CommandLineParser::HelpOption::optionHelp() const
{
	return "Shows this text";
}


// class VersionOption

void CommandLineParser::VersionOption::parseOption(
	const std::string& /*option*/, std::span<std::string>& /*cmdLine*/)
{
	std::cout << Version::full() << "\n"
	             "flavour: " << BUILD_FLAVOUR << "\n"
	             "components: " << BUILD_COMPONENTS << '\n';
	auto& parser = OUTER(CommandLineParser, versionOption);
	parser.parseStatus = CommandLineParser::Status::EXIT;
}

std::string_view CommandLineParser::VersionOption::optionHelp() const
{
	return "Prints openMSX version and exits";
}


// Setup option

void CommandLineParser::SetupOption::parseOption(
	const std::string& option, std::span<std::string>& cmdLine)
{
	auto& parser = OUTER(CommandLineParser, setupOption);
	if (parser.haveConfig) {
		throw FatalError("Only one -setup or -machine option allowed");
	}

	// resolve the filename
	auto context = userDataFileContext(Reactor::SETUP_DIR);
	const auto fileNameArg = getArgument(option, cmdLine);
	const std::string filename = context.resolve(tmpStrCat(fileNameArg, Reactor::SETUP_EXTENSION));

	try {
		parser.reactor.switchMachineFromSetup(filename);
	} catch (MSXException& e) {
		throw FatalError(std::move(e).getMessage());
	}
	parser.haveConfig = true;
}

std::string_view CommandLineParser::SetupOption::optionHelp() const
{
	return "Use setup file specified in argument";
}

// Machine option

void CommandLineParser::MachineOption::parseOption(
	const std::string& option, std::span<std::string>& cmdLine)
{
	auto& parser = OUTER(CommandLineParser, machineOption);
	if (parser.haveConfig) {
		throw FatalError("Only one -setup or -machine option allowed");
	}
	try {
		parser.reactor.switchMachine(getArgument(option, cmdLine));
	} catch (MSXException& e) {
		throw FatalError(std::move(e).getMessage());
	}
	parser.haveConfig = true;
}

std::string_view CommandLineParser::MachineOption::optionHelp() const
{
	return "Use machine specified in argument";
}


// class SettingOption

void CommandLineParser::SettingOption::parseOption(
	const std::string& option, std::span<std::string>& cmdLine)
{
	auto& parser = OUTER(CommandLineParser, settingOption);
	if (parser.haveSettings) {
		throw FatalError("Only one -setting option allowed");
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

std::string_view CommandLineParser::SettingOption::optionHelp() const
{
	return "Load an alternative settings file";
}


// class TestConfigOption

void CommandLineParser::TestConfigOption::parseOption(
	const std::string& /*option*/, std::span<std::string>& /*cmdLine*/)
{
	auto& parser = OUTER(CommandLineParser, testConfigOption);
	parser.parseStatus = CommandLineParser::Status::TEST;
}

std::string_view CommandLineParser::TestConfigOption::optionHelp() const
{
	return "Test if the specified config works and exit";
}

// class BashOption

void CommandLineParser::BashOption::parseOption(
	const std::string& /*option*/, std::span<std::string>& cmdLine)
{
	auto& parser = OUTER(CommandLineParser, bashOption);
	std::string_view last = cmdLine.empty() ? std::string_view{} : cmdLine.front();
	cmdLine = cmdLine.subspan(0, 0); // eat all remaining parameters

	if (last == "-machine") {
		for (const auto& s : Reactor::getHwConfigs("machines")) {
			std::cout << s << '\n';
		}
	} else if (last == "-setup") {
		std::vector<std::string> entries = Reactor::getSetups();
		for (const auto& s : entries) {
			std::cout << s << '\n';
		}
	} else if (last.starts_with("-ext")) {
		for (const auto& s : Reactor::getHwConfigs("extensions")) {
			std::cout << s << '\n';
		}
	} else if (last == "-romtype") {
		for (const auto& s : RomInfo::getAllRomTypes()) {
			std::cout << s << '\n';
		}
	} else {
		for (const auto& option : parser.options) {
			std::cout << option.name << '\n';
		}
	}
	parser.parseStatus = CommandLineParser::Status::EXIT;
}

std::string_view CommandLineParser::BashOption::optionHelp() const
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
	std::span<const TclObject> tokens, TclObject& result) const
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

std::string CommandLineParser::FileTypeCategoryInfoTopic::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns the file type category for the given file.";
}

} // namespace openmsx
