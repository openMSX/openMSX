#include "CDImageCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::string;

namespace openmsx {

CDImageCLI::CDImageCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-cda", *this);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

void CDImageCLI::parseOption(const string& option, array_ref<string>& cmdLine)
{
	string_view cd = string_view(option).substr(1); // cda
	string filename = getArgument(option, cmdLine);
	if (!parser.getGlobalCommandController().hasCommand(cd)) { // TODO WIP
		throw MSXException("No CDROM named '", cd, "'.");
	}
	TclObject command;
	command.addListElement(cd);
	command.addListElement(filename);
	command.executeCommand(parser.getInterpreter());
}
string_view CDImageCLI::optionHelp() const
{
	return "Use iso image in argument for the CDROM extension";
}

} // namespace openmsx
