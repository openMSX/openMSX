// $Id$

#include "DiskImageCLI.hh"
#include "SettingsConfig.hh"
#include "xmlx.hh"
#include "FileContext.hh"

using std::ostringstream;

namespace openmsx {

DiskImageCLI::DiskImageCLI(CommandLineParser& cmdLineParser)
{
	cmdLineParser.registerOption("-diska", this);
	cmdLineParser.registerOption("-diskb", this);

	cmdLineParser.registerFileClass("diskimage", this);
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
	auto_ptr<XMLElement> config(new XMLElement("config"));
	config->addAttribute("id", string("disk") + driveLetter);
	config->addChild(
		auto_ptr<XMLElement>(new XMLElement("filename", filename)));
	
	UserFileContext context;
	SettingsConfig::instance().loadConfig(context, config);
	driveLetter++;
}
const string& DiskImageCLI::fileTypeHelp() const
{
	static const string text("Disk image");
	return text;
}

} // namespace openmsx
