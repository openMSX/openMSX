// $Id$

#include "DiskImageCLI.hh"
#include "MSXConfig.hh"
#include "libxmlx/xmlx.hh"
#include "FileContext.hh"


DiskImageCLI::DiskImageCLI()
{
	CommandLineParser::instance()->registerOption("-diska", this);
	CommandLineParser::instance()->registerOption("-diskb", this);

	CommandLineParser::instance()->registerFileType("dsk", this);
	CommandLineParser::instance()->registerFileType("di1", this);
	CommandLineParser::instance()->registerFileType("di2", this);
	CommandLineParser::instance()->registerFileType("xsa", this);

	driveLetter = 'a';
}

void DiskImageCLI::parseOption(const std::string &option,
                         std::list<std::string> &cmdLine)
{
	driveLetter = option[5];	// -disk_
	parseFileType(getArgument(option, cmdLine));
}
const std::string& DiskImageCLI::optionHelp() const
{
	static const std::string text("Insert the disk image specified in argument");
	return text;
}

void DiskImageCLI::parseFileType(const std::string &filename_)
{
	std::string filename(filename_); XML::Escape(filename);
	std::ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << "<config id=\"disk" << driveLetter << "\">";
	s << "<parameter name=\"filename\">" << filename << "</parameter>";
	s << "</config>";
	s << "</msxconfig>";
	
	MSXConfig *config = MSXConfig::instance();
	config->loadStream(new UserFileContext(), s);
	driveLetter++;
}
const std::string& DiskImageCLI::fileTypeHelp() const
{
	static const std::string text("Disk image");
	return text;
}
