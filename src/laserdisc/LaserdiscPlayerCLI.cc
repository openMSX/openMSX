// $Id$

#include "LaserdiscPlayerCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "MSXException.hh"
#include "TclObject.hh"

using std::deque;
using std::string;

namespace openmsx {

LaserdiscPlayerCLI::LaserdiscPlayerCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getGlobalCommandController())
{
	commandLineParser.registerOption("-laserdisc", *this);
	commandLineParser.registerFileClass("laserdiscimage", *this);
}

bool LaserdiscPlayerCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
	return true;
}

string_ref LaserdiscPlayerCLI::optionHelp() const
{
	return "Put laserdisc image specified in argument in "
	       "virtual laserdiscplayer";
}

void LaserdiscPlayerCLI::parseFileType(const string& filename,
                                      deque<string>& /*cmdLine*/)
{
	if (!commandController.hasCommand("laserdiscplayer")) {
		throw MSXException("No laserdiscplayer.");
	}
	TclObject command(commandController.getInterpreter());
	command.addListElement("laserdiscplayer");
	command.addListElement("insert");
	command.addListElement(filename);
	command.executeCommand();
}

string_ref LaserdiscPlayerCLI::fileTypeHelp() const
{
	return "Laserdisc image, Ogg Vorbis/Theora";
}

} // namespace openmsx
