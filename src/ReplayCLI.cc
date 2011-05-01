// $Id$

#include "ReplayCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "MSXException.hh"
#include "TclObject.hh"
#include "Filename.hh"
#include "FileContext.hh"

using std::deque;
using std::string;

namespace openmsx {

ReplayCLI::ReplayCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getGlobalCommandController())
{
	commandLineParser.registerOption("-replay", *this);
	commandLineParser.registerFileClass("openMSX replay", *this);
}

bool ReplayCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
	return true;
}

const string& ReplayCLI::optionHelp() const
{
	static const string text(
	  "Load replay and start replaying it in view only mode");
	return text;
}

void ReplayCLI::parseFileType(const string& filename,
                                      deque<string>& /*cmdLine*/)
{
	TclObject command(commandController.getInterpreter());
	command.addListElement("reverse");
	command.addListElement("loadreplay");
	command.addListElement("-viewonly");
	// resolve also current dir file context...
	CurrentDirFileContext context;
	Filename fileName(Filename(filename, context));
	command.addListElement(fileName.getResolved());
	command.executeCommand();
}

const string& ReplayCLI::fileTypeHelp() const
{
	static const string text(
		"openMSX replay");
	return text;
}

} // namespace openmsx
