// $Id$

/*
   openmsx - Emulate the MSX standard.

   Copyright (C) 2001 David Heremans

*/

#include <sys/types.h>
// #include "system.h" -> this bugs g++-3.0, do we need it?

#include "config.h"

// #define EXIT_FAILURE 1

#include "msxconfig.hh"

//David Heremans
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

//#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
//#include "MSXSimple64KB.hh"
//#include "MSXRom16KB.hh"
#include "devicefactory.hh"
#include "EventDistributor.hh"
#include "KeyEventInserter.hh"

static int iconColours[] = {
	0x00000000,
	0xFF000000,
	0xFFFFFFFF,
	0xFFDBDB24,
	};
static char iconData[] = {
	0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,
	0,0,0,0,0,0,1,1,1,2,2,1,2,1,0,0,
	0,0,0,0,0,1,1,1,2,2,1,2,1,2,0,0,
	0,0,0,0,0,1,1,1,2,2,1,2,1,2,0,0,
	0,1,1,1,0,1,1,1,1,3,3,3,3,3,3,0,
	1,1,1,1,1,1,1,1,3,3,3,3,1,3,3,0,
	1,0,1,1,1,1,1,1,1,3,3,3,3,3,1,0,
	0,0,0,1,1,1,1,1,2,2,2,2,2,1,1,1,
	0,0,0,1,1,1,1,2,2,2,2,2,2,2,1,1,
	1,0,1,1,1,1,2,2,2,2,2,2,2,1,1,0,
	1,1,1,1,1,1,2,2,2,2,2,2,2,2,0,0,
	1,1,1,1,1,2,2,2,2,2,2,1,2,0,0,0,
	0,1,1,1,1,2,2,2,2,3,3,3,3,0,0,0,
	0,0,0,1,1,1,1,2,3,3,3,3,3,0,0,0,
	0,1,1,1,1,1,1,1,3,3,3,1,1,1,1,0,
	0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
	};

int eventDistributorStarter(void* parm)
{
	EventDistributor::instance()->run();
	return 0;	// doesn't return
}


int main (int argc, char **argv)
{
	char *configfile="msxconfig.xml";
	SDL_Thread *thread;
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
	SDL_WM_SetCaption("openMSX " VERSION " [alpha]", 0);

	// Set icon
	static int iconRGBA[256];
	for (int i = 0; i < 256; i++) {
		iconRGBA[i] = iconColours[iconData[i]];
	}
	SDL_Surface *iconSurf = SDL_CreateRGBSurfaceFrom(
		iconRGBA, 16, 16, 32, 64,
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	SDL_SetColorKey(iconSurf, SDL_SRCCOLORKEY, 0);
	SDL_WM_SetIcon(iconSurf, NULL);

	try {
		EmuTime zero;
		for (std::list<MSXConfig::Device*>::const_iterator j=MSXConfig::instance()->deviceList.begin();
		     j != MSXConfig::instance()->deviceList.end();
		     j++) {
			(*j)->dump();
			MSXDevice *device = deviceFactory::create((*j), zero);
			assert (device != 0);
			MSXMotherBoard::instance()->addDevice(device);
			PRT_DEBUG ("Instantiated:" << (*j)->getType());
		}

		// Start a new thread for event handling
		thread=SDL_CreateThread(eventDistributorStarter, 0);

		//It works!! But commented out because this is annoying
		//keyi << "... key inserter test ...";
		//keyi.flush();

		PRT_DEBUG ("starting MSX");
		MSXMotherBoard::instance()->StartMSX();
		// When we return we clean everything up
		SDL_KillThread(thread);
		MSXMotherBoard::instance()->DestroyMSX();
	} catch (MSXException e) {
		std::cerr << e.desc << std::endl;
		exit(1);
	}

	exit (0);
}
