// $Id$

#ifndef __COMMANDLINEPARSER_HH__
#define __COMMANDLINEPARSER_HH__

#include "openmsx.hh"
#include "config.h"
#include "MSXConfig.hh"

class CommandLineParser
{
	public:
	static int checkFileType(char* parameter,char &diskLetter,byte &cartridge);
	static int checkFileExt(char* filename);
	static void parse(MSXConfig::Backend* config,int argc, char **argv);
};

#endif
