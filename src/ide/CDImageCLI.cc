// $Id$

#include "CDImageCLI.hh"
#include "Reactor.hh"
#include "CommandController.hh"
#include "CommandLineParser.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::list;
using std::string;

namespace openmsx {

CDImageCLI::CDImageCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getReactor().getCommandController())
{
	commandLineParser.registerOption("-cda", *this);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

bool CDImageCLI::parseOption(const string& option, list<string>& cmdLine)
{
	string cd = option.substr(1); // cda
	string filename = getArgument(option, cmdLine);
	if (!commandController.hasCommand(cd)) {
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
