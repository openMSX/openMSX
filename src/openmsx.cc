/* 
   openmsx - Emulate the MSX standard.

   Copyright (C) 2001 David Heremans

*/

#include <stdio.h>
#include <sys/types.h>
#include "system.h"

#define EXIT_FAILURE 1

#include "msxconfig.hh"

int
main (int argc, char **argv)
{
	if (argc<2) {
		cerr << "Usage: " << argv[0] << " msxconfig.xml" << endl;
		exit(1);
	}
	// this is mainly for me (Joost), for testing the xml parsing
	try
	{
		MSXConfig::instance()->loadFile(argv[1]);
	}
	catch (MSXException e)
	{
		cerr << e.desc << endl;
		exit(1);
	}
	list<MSXConfig::Device*>::const_iterator i=MSXConfig::instance()->deviceList.begin();
	for (; i != MSXConfig::instance()->deviceList.end(); i++)
	{
		(*i)->dump();
	}
	
	exit (0);
}

/*
static void
usage (int status)
{
  printf (_("%s - \
Emulate the MSX standard.\n"), program_name);
  printf (_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
  printf (_("\
Options:
  -q, --quiet, --silent      inhibit usual output
  --verbose                  print more information
  -h, --help                 display this help and exit\n\
  -V, --version              output version information and exit\n\
"));
  exit (status);
}
*/
