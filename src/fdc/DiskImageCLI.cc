// $Id$

#include "DiskImageCLI.hh"
#include "MSXConfig.hh"
#include "libxmlx/xmlx.hh"
#include "FileContext.hh"


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

void DiskImageCLI::parseFileType(const string &filename_)
{
	string filename(filename_); XML::Escape(filename);
	ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << "<config id=\"disk" << driveLetter << "\">";
	s << "<parameter name=\"filename\">" << filename << "</parameter>";
	s << "</config>";
	s << "</msxconfig>";
	
	UserFileContext context;
	MSXConfig::instance().loadStream(context, s);
	driveLetter++;
}
const string& DiskImageCLI::fileTypeHelp() const
{
	static const string text("Disk image");
	return text;
}

} // namespace openmsx
