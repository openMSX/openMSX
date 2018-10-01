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

void LaserdiscPlayerCLI::parseOption(const string& option, array_ref<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

string_view LaserdiscPlayerCLI::optionHelp() const
{
	return "Put laserdisc image specified in argument in "
	       "virtual laserdiscplayer";
}

void LaserdiscPlayerCLI::parseFileType(const string& filename,
                                       array_ref<string>& /*cmdLine*/)
{
	if (!parser.getGlobalCommandController().hasCommand("laserdiscplayer")) {
		throw MSXException("No laserdiscplayer.");
	}
	TclObject command;
	command.addListElement("laserdiscplayer");
	command.addListElement("insert");
	command.addListElement(filename);
	command.executeCommand(parser.getInterpreter());
}

string_view LaserdiscPlayerCLI::fileTypeHelp() const
{
	return "Laserdisc image, Ogg Vorbis/Theora";
}

} // namespace openmsx
