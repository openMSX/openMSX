// $Id$

#include "DiskImageCLI.hh"
#include "MSXMotherBoard.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"
#include "XMLElement.hh"
#include "FileContext.hh"
#include "MSXException.hh"

using std::list;
using std::string;

namespace openmsx {

DiskImageCLI::DiskImageCLI(CommandLineParser& commandLineParser)
	: commandController(commandLineParser.getMotherBoard().getCommandController())
{
	commandLineParser.registerOption("-diska", this);
	commandLineParser.registerOption("-diskb", this);

	commandLineParser.registerFileClass("diskimage", this);
	driveLetter = 'a';
}

bool DiskImageCLI::parseOption(const string& option,
                               list<string>& cmdLine)
{
	string filename = getArgument(option, cmdLine);
	parse(option.substr(1), filename, cmdLine);
	return true;
}
const string& DiskImageCLI::optionHelp() const
{
	static const string text("Insert the disk image specified in argument");
	return text;
}

void DiskImageCLI::parseFileType(const string& filename,
                                 list<string>& cmdLine)
{
	parse(string("disk") + driveLetter, filename, cmdLine);
	++driveLetter;
}

const string& DiskImageCLI::fileTypeHelp() const
{
	static const string text("Disk image");
	return text;
}

void DiskImageCLI::parse(const string& drive, const string& image,
                         list<string>& cmdLine)
{
	if (!commandController.hasCommand(drive)) {
		throw MSXException("No drive named '" + drive + "'.");
	}
	string command = drive + ' ' + image;
	while (peekArgument(cmdLine) == "-ips") {
		cmdLine.pop_front();
		command += ' ' + getArgument("-ips", cmdLine);
	}
	commandController.executeCommand(command);
}

} // namespace openmsx
