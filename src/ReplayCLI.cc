#include "ReplayCLI.hh"

#include "CommandLineParser.hh"
#include "TclObject.hh"

#include <array>

namespace openmsx {

ReplayCLI::ReplayCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-replay", *this);
	parser.registerFileType(std::array<std::string_view, 1>{"omr"}, *this);
}

void ReplayCLI::parseOption(const std::string& option, std::span<std::string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

std::string_view ReplayCLI::optionHelp() const
{
	return "Load replay and start replaying it in view only mode";
}

void ReplayCLI::parseFileType(const std::string& filename,
                              std::span<std::string>& /*cmdLine*/)
{
	TclObject command = makeTclList("reverse", "loadreplay", "-viewonly", filename);
	command.executeCommand(parser.getInterpreter());
}

std::string_view ReplayCLI::fileTypeHelp() const
{
	return "openMSX replay";
}

std::string_view ReplayCLI::fileTypeCategoryName() const
{
	return "replay";
}

} // namespace openmsx
