/* 
   openmsx - Emulate the MSX standard.

   Copyright (C) 2001 David Heremans

*/

#include <stdio.h>
#include <sys/types.h>
#include "system.h"

#define EXIT_FAILURE 1

#include "msxconfig.hh"


//David Heremans
#include <iostream>
#include <fstream.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXZ80.hh"
#include "MSXSimple64KB.hh"
#include "MSXRom16KB.hh"
#include "devicefactory.hh"

//David Heremans

int main (int argc, char **argv)
{
	char *configfile="msxconfig.xml";

	if (argc<2) {
		PRT_INFO ("Using " << configfile << " as default configuration file.");
	} else {
		configfile=argv[1];
	}
	// this is mainly for me (Joost), for testing the xml parsing
	try {
		MSXConfig::instance()->loadFile(configfile);
	
		list<MSXConfig::Device*>::const_iterator i=MSXConfig::instance()->deviceList.begin();
		for (; i != MSXConfig::instance()->deviceList.end(); i++) {
			(*i)->dump();
		}
	} catch (MSXException e) {
		cerr << e.desc << endl;
		exit(1);
	}
	// End of Joosts test routine 
	// Here comes the Real Stuff(tm) :-)
	// (Actualy David's test stuff)
	
	//Werkt SDL ?
	if (SDL_Init(SDL_INIT_VIDEO)<0) {
		PRT_ERROR("Couldn't init SDL: " << SDL_GetError());
	}
	atexit(SDL_Quit);

	// Een moederbord als eerste
	MSXMotherBoard *moederbord = MSXMotherBoard::instance();
	// en dan nu al de devices die volgens de xml nodig zijn
	list<MSXConfig::Device*>::const_iterator j=MSXConfig::instance()->deviceList.begin();
	for (; j != MSXConfig::instance()->deviceList.end(); j++) {
		(*j)->dump();
		MSXDevice *device = deviceFactory::create( (*j) );
		if (device != 0){
			moederbord->addDevice(device);
		PRT_DEBUG ("---------------------------\nfactory:" << (*j)->getType());
		}
	}
	
	PRT_DEBUG ("Initing MSX");
	moederbord->InitMSX();
	
	PRT_DEBUG ("starting MSX");
	moederbord->StartMSX();

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
