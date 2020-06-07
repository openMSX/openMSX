#include "LaserdiscPlayerCLI.hh"
#include "CommandLineParser.hh"
#include "Interpreter.hh"
#include "MSXException.hh"
#include "TclObject.hh"

namespace openmsx {

LaserdiscPlayerCLI::LaserdiscPlayerCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-laserdisc", *this);
	parser.registerFileType({"ogv"}, *this);
}

void LaserdiscPlayerCLI::parseOption(const std::string& option, std::span<std::string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
}

std::string_view LaserdiscPlayerCLI::optionHelp() const
{
	return "Put LaserDisc image specified in argument in "
	       "virtual LaserDisc player";
}

void LaserdiscPlayerCLI::parseFileType(const std::string& filename,
                                       std::span<std::string>& /*cmdLine*/)
{
	if (!parser.getInterpreter().hasCommand("laserdiscplayer")) {
		throw MSXException("No LaserDisc player present.");
	}
	TclObject command = makeTclList("laserdiscplayer", "insert", filename);
	command.executeCommand(parser.getInterpreter());
}

std::string_view LaserdiscPlayerCLI::fileTypeHelp() const
{
	return "LaserDisc image, Ogg Vorbis/Theora";
}

std::string_view LaserdiscPlayerCLI::fileTypeCategoryName() const
{
	return "laserdisc";
}

} // namespace openmsx
