// $Id$

#ifndef __DISKIMAGEMANAGER_HH__
#define __DISKIMAGEMANAGER_HH__

#include <string>
#include "CommandLineParser.hh"


class DiskImageCLI : public CLIOption, public CLIFileType
{
	public:
		DiskImageCLI();
		virtual void parseOption(const string &option,
			list<string> &cmdLine);
		virtual const string& optionHelp() const;
		virtual void parseFileType(const string &filename);
		virtual const string& fileTypeHelp() const;

	private:
		char driveLetter;
};

#endif
