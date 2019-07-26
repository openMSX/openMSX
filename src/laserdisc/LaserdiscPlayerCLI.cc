#include "LaserdiscPlayerCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "MSXException.hh"
#include "TclObject.hh"

using std::string;

namespace openmsx {

LaserdiscPlayerCLI::LaserdiscPlayerCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-laserdisc", *this);
	parser.registerFileType("ogv", *this);
}

void LaserdiscPlayerCLI::parseOption(const string& option, span<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

std::string_view LaserdiscPlayerCLI::optionHelp() const
{
	return "Put laserdisc image specified in argument in "
	       "virtual laserdiscplayer";
}

void LaserdiscPlayerCLI::parseFileType(const string& filename,
                                       span<string>& /*cmdLine*/)
{
	if (!parser.getGlobalCommandController().hasCommand("laserdiscplayer")) {
		throw MSXException("No laserdiscplayer.");
	}
	TclObject command = makeTclList("laserdiscplayer", "insert", filename);
	command.executeCommand(parser.getInterpreter());
}

std::string_view LaserdiscPlayerCLI::fileTypeHelp() const
{
	return "Laserdisc image, Ogg Vorbis/Theora";
}

} // namespace openmsx
