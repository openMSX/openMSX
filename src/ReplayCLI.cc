#include "ReplayCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"

using std::deque;
using std::string;

namespace openmsx {

ReplayCLI::ReplayCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-replay", *this);
	parser.registerFileType("omr", *this);
}

void ReplayCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

string_ref ReplayCLI::optionHelp() const
{
	return "Load replay and start replaying it in view only mode";
}

void ReplayCLI::parseFileType(const string& filename,
                                      deque<string>& /*cmdLine*/)
{
	TclObject command(parser.getGlobalCommandController().getInterpreter());
	command.addListElement("reverse");
	command.addListElement("loadreplay");
	command.addListElement("-viewonly");
	command.addListElement(filename);
	command.executeCommand();
}

string_ref ReplayCLI::fileTypeHelp() const
{
	return "openMSX replay";
}

} // namespace openmsx
