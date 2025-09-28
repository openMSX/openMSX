#include "CDImageCLI.hh"

#include "CommandLineParser.hh"
#include "Interpreter.hh"
#include "MSXException.hh"
#include "TclObject.hh"

namespace openmsx {

CDImageCLI::CDImageCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-cda", *this);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

void CDImageCLI::parseOption(const std::string& option, std::span<std::string>& cmdLine)
{
	auto cd = std::string_view(option).substr(1); // cda
	std::string filename = getArgument(option, cmdLine);
	if (!parser.getInterpreter().hasCommand(std::string(cd))) { // TODO WIP
		throw MSXException("No CD-ROM named '", cd, "'.");
	}
	TclObject command = makeTclList(cd, filename);
	command.executeCommand(parser.getInterpreter());
}
std::string_view CDImageCLI::optionHelp() const
{
	return "Use iso image in argument for the CD-ROM extension";
}

} // namespace openmsx
