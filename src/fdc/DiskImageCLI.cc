// $Id$

#include "DiskImageCLI.hh"
#include "CommandLineParser.hh"
#include "GlobalCommandController.hh"
#include "TclObject.hh"
#include "MSXException.hh"

using std::deque;
using std::string;

namespace openmsx {

DiskImageCLI::DiskImageCLI(CommandLineParser& parser_)
	: parser(parser_)
{
	parser.registerOption("-diska", *this);
	parser.registerOption("-diskb", *this);

	parser.registerFileClass("diskimage", *this);
	driveLetter = 'a';
}

bool DiskImageCLI::parseOption(const string& option, deque<string>& cmdLine)
{
	string filename = getArgument(option, cmdLine);
	parse(string_ref(option).substr(1), filename, cmdLine);
	return true;
}
string_ref DiskImageCLI::optionHelp() const
{
	return "Insert the disk image specified in argument";
}

void DiskImageCLI::parseFileType(const string& filename, deque<string>& cmdLine)
{
	parse(string("disk") + driveLetter, filename, cmdLine);
	++driveLetter;
}

string_ref DiskImageCLI::fileTypeHelp() const
{
	return "Disk image";
}

void DiskImageCLI::parse(string_ref drive, string_ref image,
                         deque<string>& cmdLine)
{
	if (!parser.getGlobalCommandController().hasCommand(drive)) { // TODO WIP
		throw MSXException("No drive named '" + drive + "'.");
	}
	TclObject command(parser.getGlobalCommandController().getInterpreter());
	command.addListElement(drive);
	command.addListElement(image);
	while (peekArgument(cmdLine) == "-ips") {
		cmdLine.pop_front();
		command.addListElement(getArgument("-ips", cmdLine));
	}
	command.executeCommand();
}

} // namespace openmsx
