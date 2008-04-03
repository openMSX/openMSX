// $Id$

#include "HDImageCLI.hh"
#include "CommandController.hh"
#include "CommandLineParser.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::deque;
using std::string;

namespace openmsx {

HDImageCLI::HDImageCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getCommandController())
{
	commandLineParser.registerOption("-hda", *this);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

bool HDImageCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	string hd = option.substr(1); // hda
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
const string& HDImageCLI::optionHelp() const
{
	static const string text("Use hard disk image in argument for the IDE or SCSI extensions");
	return text;
}

} // namespace openmsx
