#include "HDImageCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::string;

namespace openmsx {

HDImageCLI::HDImageCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-hda", *this);
	// TODO: offer more options in case you want to specify 2 hard disk images?
}

void HDImageCLI::parseOption(const string& option, array_ref<string>& cmdLine)
{
	string_ref hd = string_ref(option).substr(1); // hda
	string filename = getArgument(option, cmdLine);
	if (!parser.getGlobalCommandController().hasCommand(hd)) { // TODO WIP
		throw MSXException("No hard disk named '" + hd + "'.");
	}
	TclObject command(parser.getGlobalCommandController().getInterpreter());
	command.addListElement(hd);
	command.addListElement(filename);
	command.executeCommand();
}
string_ref HDImageCLI::optionHelp() const
{
	return "Use hard disk image in argument for the IDE or SCSI extensions";
}

} // namespace openmsx
