#include "DiskImageCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::string;

namespace openmsx {

DiskImageCLI::DiskImageCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-diska", *this);
	parser.registerOption("-diskb", *this);
	parser.registerFileType("di1,di2,dmk,dsk,xsa", *this);
	driveLetter = 'a';
}

void DiskImageCLI::parseOption(const string& option, array_ref<string>& cmdLine)
{
	string filename = getArgument(option, cmdLine);
	parse(string_view(option).substr(1), filename, cmdLine);
}
string_view DiskImageCLI::optionHelp() const
{
	return "Insert the disk image specified in argument";
}

void DiskImageCLI::parseFileType(const string& filename, array_ref<string>& cmdLine)
{
	parse(strCat("disk", driveLetter), filename, cmdLine);
	++driveLetter;
}

string_view DiskImageCLI::fileTypeHelp() const
{
	return "Disk image";
}

void DiskImageCLI::parse(string_view drive, string_view image,
                         array_ref<string>& cmdLine)
{
	if (!parser.getGlobalCommandController().hasCommand(drive)) { // TODO WIP
		throw MSXException("No drive named '", drive, "'.");
	}
	TclObject command;
	command.addListElement(drive);
	command.addListElement(image);
	while (peekArgument(cmdLine) == "-ips") {
		cmdLine.pop_front();
		command.addListElement(getArgument("-ips", cmdLine));
	}
	command.executeCommand(parser.getInterpreter());
}

} // namespace openmsx
