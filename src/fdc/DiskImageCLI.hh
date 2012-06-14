// $Id$

#ifndef DISKIMAGEMANAGER_HH
#define DISKIMAGEMANAGER_HH

#include "CLIOption.hh"
#include "string_ref.hh"

namespace openmsx {

class CommandLineParser;
class GlobalCommandController;

class DiskImageCLI : public CLIOption, public CLIFileType
{
public:
	explicit DiskImageCLI(CommandLineParser& cmdLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::deque<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;
	virtual void parseFileType(const std::string& filename,
	                           std::deque<std::string>& cmdLine);
	virtual const std::string& fileTypeHelp() const;

private:
	void parse(string_ref drive, string_ref image,
	           std::deque<std::string>& cmdLine);

	GlobalCommandController& commandController;
	char driveLetter;
};

} // namespace openmsx

#endif
