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

void CDImageCLI::parseOption(const string& option, span<string>& cmdLine)
{
	auto cd = std::string_view(option).substr(1); // cda
	string filename = getArgument(option, cmdLine);
	if (!parser.getGlobalCommandController().hasCommand(cd)) { // TODO WIP
		throw MSXException("No CDROM named '", cd, "'.");
	}
	TclObject command = makeTclList(cd, filename);
	command.executeCommand(parser.getInterpreter());
}
std::string_view CDImageCLI::optionHelp() const
{
	return "Use iso image in argument for the CDROM extension";
}

} // namespace openmsx
