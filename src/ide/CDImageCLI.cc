#include "CDImageCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::deque;
using std::string;

namespace openmsx {

CDImageCLI::CDImageCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-cda", *this);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

void CDImageCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	string_ref cd = string_ref(option).substr(1); // cda
	string filename = getArgument(option, cmdLine);
	if (!parser.getGlobalCommandController().hasCommand(cd)) { // TODO WIP
		throw MSXException("No CDROM named '" + cd + "'.");
	}
	TclObject command(parser.getGlobalCommandController().getInterpreter());
	command.addListElement(cd);
	command.addListElement(filename);
	command.executeCommand();
}
string_ref CDImageCLI::optionHelp() const
{
	return "Use iso image in argument for the CDROM extension";
}

} // namespace openmsx
