// $Id$

#ifndef __DISKIMAGEMANAGER_HH__
#define __DISKIMAGEMANAGER_HH__

#include "CommandLineParser.hh"

namespace openmsx {

class DiskImageCLI : public CLIOption, public CLIFileType
{
public:
	DiskImageCLI(CommandLineParser& cmdLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::list<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;
	virtual void parseFileType(const std::string& filename,
	                           std::list<std::string>& cmdLine);
	virtual const std::string& fileTypeHelp() const;

private:
	char driveLetter;
};

} // namespace openmsx

#endif
