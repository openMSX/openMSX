// $Id$

#ifndef DISKIMAGEMANAGER_HH
#define DISKIMAGEMANAGER_HH

#include "CLIOption.hh"
#include "string_ref.hh"

namespace openmsx {

class CommandLineParser;

class DiskImageCLI : public CLIOption, public CLIFileType
{
public:
	explicit DiskImageCLI(CommandLineParser& cmdLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::deque<std::string>& cmdLine);
	virtual string_ref optionHelp() const;
	virtual void parseFileType(const std::string& filename,
	                           std::deque<std::string>& cmdLine);
	virtual string_ref fileTypeHelp() const;

private:
	void parse(string_ref drive, string_ref image,
	           std::deque<std::string>& cmdLine);

	CommandLineParser& parser;
	char driveLetter;
};

} // namespace openmsx

#endif
