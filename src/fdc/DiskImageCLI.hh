// $Id$

#ifndef __DISKIMAGEMANAGER_HH__
#define __DISKIMAGEMANAGER_HH__

#include "CommandLineParser.hh"

namespace openmsx {

class DiskImageCLI : public CLIOption, public CLIFileType
{
public:
	DiskImageCLI();
	virtual bool parseOption(const string& option,
		list<string>& cmdLine);
	virtual const string& optionHelp() const;
	virtual void parseFileType(const string& filename);
	virtual const string& fileTypeHelp() const;

private:
	char driveLetter;
};

} // namespace openmsx

#endif
