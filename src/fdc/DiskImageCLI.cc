// $Id$

#include "DiskImageCLI.hh"
#include "GlobalSettings.hh"
#include "XMLElement.hh"
#include "FileContext.hh"

using std::list;
using std::string;

namespace openmsx {

DiskImageCLI::DiskImageCLI(CommandLineParser& cmdLineParser)
{
	cmdLineParser.registerOption("-diska", this);
	cmdLineParser.registerOption("-diskb", this);

	cmdLineParser.registerFileClass("diskimage", this);
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
                                 list<string>& /*cmdLine*/)
{
	XMLElement& config = GlobalSettings::instance().getMediaConfig();
	XMLElement& diskElem = config.getCreateChild(string("disk") + driveLetter);
	diskElem.setData(filename);
	diskElem.setFileContext(std::auto_ptr<FileContext>(new UserFileContext()));
	driveLetter++;
}

const string& DiskImageCLI::fileTypeHelp() const
{
	static const string text("Disk image");
	return text;
}

} // namespace openmsx
