// $Id$

/*
 *  openmsx - Emulate the MSX standard.
 *
 *  Copyright (C) 2001 David Heremans
 */

#include "config.h"
#include "msxconfig.hh"
#include <string>
#include <SDL/SDL.h>
#include "Thread.hh"
#include "MSXMotherBoard.hh"
#include "devicefactory.hh"
#include "EventDistributor.hh"
#include "EmuTime.hh"


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


void initializeSDL()
{
	if (SDL_Init(SDL_INIT_VIDEO)<0)
		PRT_ERROR("Couldn't init SDL: " << SDL_GetError());
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
}


int main (int argc, char **argv)
{
	char *configfile = "msxconfig.xml";
	if (argc<2) {
		PRT_INFO ("Using " << configfile << " as default configuration file.");
	} else {
		configfile=argv[1];
	}

	try {
		MSXConfig::instance()->loadFile(configfile);
		
		initializeSDL();
	
		EmuTime zero;
		std::list<MSXConfig::Device*>::const_iterator i;
		for (i = MSXConfig::instance()->deviceList.begin();
		     i != MSXConfig::instance()->deviceList.end();
		     i++) {
			(*i)->dump();
			MSXDevice *device = deviceFactory::create(*i, zero);
			MSXMotherBoard::instance()->addDevice(device);
			PRT_DEBUG ("Instantiated:" << (*i)->getType());
		}

		// Start a new thread for event handling
		Thread* thread = new Thread(EventDistributor::instance());
		thread->start();

		PRT_DEBUG ("starting MSX");
		MSXMotherBoard::instance()->StartMSX();
		
		// When we return we clean everything up
		thread->stop();
		MSXMotherBoard::instance()->DestroyMSX();
	} 
	catch (MSXException& e) {
		std::cerr << e.desc << std::endl;
		exit(1);
	}
	exit (0);
}


