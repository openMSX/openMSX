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
	MSXConfig::instance()->loadFile("msxconfig.xml");


	/* do the work */
	printf("Hello, World\n");

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
