// $Id$

#include "HDImageCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::deque;
using std::string;

namespace openmsx {

HDImageCLI::HDImageCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getGlobalCommandController())
{
	commandLineParser.registerOption("-hda", *this);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

bool HDImageCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	string_ref hd = string_ref(option).substr(1); // hda
	string filename = getArgument(option, cmdLine);
	if (!commandController.hasCommand(hd)) { // TODO WIP
		throw MSXException("No hard disk named '" + hd + "'.");
	}
	TclObject command(commandController.getInterpreter());
	command.addListElement(hd);
	command.addListElement(filename);
	command.executeCommand();
	return true;
}
string_ref HDImageCLI::optionHelp() const
{
	return "Use hard disk image in argument for the IDE or SCSI extensions";
}

} // namespace openmsx
