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

void ReplayCLI::parseOption(const string& option, array_ref<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

string_view ReplayCLI::optionHelp() const
{
	return "Load replay and start replaying it in view only mode";
}

void ReplayCLI::parseFileType(const string& filename,
                              array_ref<string>& /*cmdLine*/)
{
	TclObject command;
	command.addListElement("reverse");
	command.addListElement("loadreplay");
	command.addListElement("-viewonly");
	command.addListElement(filename);
	command.executeCommand(parser.getInterpreter());
}

string_view ReplayCLI::fileTypeHelp() const
{
	return "openMSX replay";
}

} // namespace openmsx
