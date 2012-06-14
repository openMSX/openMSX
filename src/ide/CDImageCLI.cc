// $Id$

#include "CDImageCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::deque;
using std::string;

namespace openmsx {

CDImageCLI::CDImageCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getGlobalCommandController())
{
	commandLineParser.registerOption("-cda", *this);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

bool CDImageCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	string_ref cd = string_ref(option).substr(1); // cda
	string filename = getArgument(option, cmdLine);
	if (!commandController.hasCommand(cd)) { // TODO WIP
		throw MSXException("No CDROM named '" + cd + "'.");
	}
	TclObject command(commandController.getInterpreter());
	command.addListElement(cd);
	command.addListElement(filename);
	command.executeCommand();
	return true;
}
const string& CDImageCLI::optionHelp() const
{
	static const string text("Use iso image in argument for the CDROM extension");
	return text;
}

} // namespace openmsx
