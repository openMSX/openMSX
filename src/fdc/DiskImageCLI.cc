// $Id$

#include "DiskImageCLI.hh"
#include "MSXConfig.hh"
#include "xmlx.hh"
#include "FileContext.hh"

using std::ostringstream;

namespace openmsx {

DiskImageCLI::DiskImageCLI()
{
	CommandLineParser::instance().registerOption("-diska", this);
	CommandLineParser::instance().registerOption("-diskb", this);

	CommandLineParser::instance().registerFileClass("diskimages", this);
	driveLetter = 'a';
}

bool DiskImageCLI::parseOption(const string &option,
                         list<string> &cmdLine)
{
	driveLetter = option[5];	// -disk_
	parseFileType(getArgument(option, cmdLine));
	return true;
}
const string& DiskImageCLI::optionHelp() const
{
	static const string text("Insert the disk image specified in argument");
	return text;
}

void DiskImageCLI::parseFileType(const string &filename)
{
	XMLElement config("config");
	config.addAttribute("id", string("disk") + driveLetter);
	XMLElement* parameter = new XMLElement("parameter", filename);
	parameter->addAttribute("name", "filename");
	config.addChild(parameter);
	
	UserFileContext context;
	MSXConfig::instance().loadConfig(config, context);
	driveLetter++;
}
const string& DiskImageCLI::fileTypeHelp() const
{
	static const string text("Disk image");
	return text;
}

} // namespace openmsx
