// $Id$

/* 
   openmsx - Emulate the MSX standard.

   Copyright (C) 2001 David Heremans

*/

#include <sys/types.h>
// #include "system.h" -> this bugs g++-3.0, do we need it?

// #define EXIT_FAILURE 1

#include "msxconfig.hh"


//David Heremans
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXSimple64KB.hh"
#include "MSXRom16KB.hh"
#include "devicefactory.hh"
#include "EventDistributor.hh"
#include "KeyEventInserter.hh"

int eventDistributorStarter(void* parm)
{
	EventDistributor::instance()->run();
	return 0;	// doesn't return
}


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
	
		std::list<MSXConfig::Config*>::const_iterator i=MSXConfig::instance()->configList.begin();
		for (; i != MSXConfig::instance()->configList.end(); i++) {
			(*i)->dump();
		}
	} catch (MSXException e) {
		std::cerr << e.desc << std::endl;
		exit(1);
	}
	// End of Joosts test routine 
	// Here comes the Real Stuff(tm) :-)
	// (Actualy David's test stuff)
	
	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO)<0) {
		PRT_ERROR("Couldn't init SDL: " << SDL_GetError());
	}
	atexit(SDL_Quit);

	// Een moederbord als eerste
	MSXMotherBoard *motherboard = MSXMotherBoard::instance();
	// en dan nu al de devices die volgens de xml nodig zijn
	std::list<MSXConfig::Device*>::const_iterator j=MSXConfig::instance()->deviceList.begin();
	for (; j != MSXConfig::instance()->deviceList.end(); j++) {
		(*j)->dump();
		MSXDevice *device = deviceFactory::create( (*j) );
		if (device != 0){
			motherboard->addDevice(device);
		PRT_DEBUG ("---------------------------\nfactory:" << (*j)->getType());
		}
	}
	
	PRT_DEBUG ("Initing MSX");
	motherboard->InitMSX();
	
	// Start a new thread for event handling
	SDL_CreateThread(eventDistributorStarter, 0);

	// test thingy for Joost: [doesn't do harm YET]
	keyi << "Hello";

	PRT_DEBUG ("starting MSX");
	motherboard->StartMSX();

	exit (0);
}

