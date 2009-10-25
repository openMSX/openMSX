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

const string& LaserdiscPlayerCLI::optionHelp() const
{
	static const string text(
	  "Put laserdisc image specified in argument in virtual laserdiscplayer");
	return text;
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

const string& LaserdiscPlayerCLI::fileTypeHelp() const
{
	static const string text(
		"Laserdisc image, Ogg Vorbis/Theora");
	return text;
}

} // namespace openmsx
