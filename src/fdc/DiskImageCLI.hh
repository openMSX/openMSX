// $Id$

#ifndef __DISKIMAGEMANAGER_HH__
#define __DISKIMAGEMANAGER_HH__

#include <string>
#include "CommandLineParser.hh"


class DiskImageCLI : public CLIOption, public CLIFileType
{
	public:
		DiskImageCLI();
		virtual void parseOption(const std::string &option,
			std::list<std::string> &cmdLine);
		virtual const std::string& optionHelp();
		virtual void parseFileType(const std::string &filename);
		virtual const std::string& fileTypeHelp();

	private:
		char driveLetter;
};

#endif
