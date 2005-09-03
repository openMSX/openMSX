// $Id$

#ifndef DISKIMAGEMANAGER_HH
#define DISKIMAGEMANAGER_HH

#include "CommandLineParser.hh"

namespace openmsx {

class CommandController;

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
	CommandController& commandController;
	char driveLetter;
};

} // namespace openmsx

#endif
