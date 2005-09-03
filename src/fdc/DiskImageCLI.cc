// $Id$

#include "DiskImageCLI.hh"
#include "MSXMotherBoard.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"
#include "XMLElement.hh"
#include "FileContext.hh"

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
	driveLetter = option[5];	// -disk_
	parseFileType(getArgument(option, cmdLine), cmdLine);
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
	XMLElement& config = commandController.getGlobalSettings().getMediaConfig();
	XMLElement& diskElem = config.getCreateChild(string("disk") + driveLetter);
	diskElem.getCreateChild("filename", filename);
	diskElem.setFileContext(std::auto_ptr<FileContext>(new UserFileContext(
	                                                commandController)));
	while (peekArgument(cmdLine) == "-ips") {
		cmdLine.pop_front();
		string ipsFile = getArgument("-ips", cmdLine);
		diskElem.getCreateChild("ips", ipsFile);
	}
	++driveLetter;
}

const string& DiskImageCLI::fileTypeHelp() const
{
	static const string text("Disk image");
	return text;
}

} // namespace openmsx
