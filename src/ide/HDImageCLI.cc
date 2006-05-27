// $Id$

#include "HDImageCLI.hh"
#include "Reactor.hh"
#include "CommandController.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::list;
using std::string;

namespace openmsx {

HDImageCLI::HDImageCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getReactor().getCommandController())
{
	commandLineParser.registerOption("-hda", this);
}

bool HDImageCLI::parseOption(const string& option, list<string>& cmdLine)
{
	string hd = option.substr(1); // hda
	string filename = getArgument(option, cmdLine);
	if (!commandController.hasCommand(hd)) {
		throw MSXException("No HD named '" + hd + "'.");
	}
	TclObject command(commandController.getInterpreter());
	command.addListElement(hd);
	command.addListElement(filename);
	command.executeCommand();
	return true;
}
const string& HDImageCLI::optionHelp() const
{
	static const string text("Use the specified HD image");
	return text;
}

} // namespace openmsx
