#include "ReplayCLI.hh"
#include "CommandLineParser.hh"
#include "TclObject.hh"

using std::string;

namespace openmsx {

ReplayCLI::ReplayCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-replay", *this);
	parser.registerFileType("omr", *this);
}

void ReplayCLI::parseOption(const string& option, span<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

std::string_view ReplayCLI::optionHelp() const
{
	return "Load replay and start replaying it in view only mode";
}

void ReplayCLI::parseFileType(const string& filename,
                              span<string>& /*cmdLine*/)
{
	TclObject command = makeTclList("reverse", "loadreplay", "-viewonly", filename);
	command.executeCommand(parser.getInterpreter());
}

std::string_view ReplayCLI::fileTypeHelp() const
{
	return "openMSX replay";
}

} // namespace openmsx
