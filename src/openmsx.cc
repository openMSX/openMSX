// $Id$

/*
 *  openmsx - Emulate the MSX standard.
 *
 *  Copyright (C) 2001 David Heremans
 */

#include "config.h"
#include "MSXConfig.hh"
#include <string>
#include <SDL/SDL.h>
#include "Thread.hh"
#include "MSXMotherBoard.hh"
#include "devicefactory.hh"
#include "EventDistributor.hh"
#include "EmuTime.hh"
#include "CommandLineParser.hh"
#define __OPENMSX_CC__

#include "icon.nn"

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
	// create configuration backend
	// for now there is only one, "xml" based
	MSXConfig::Backend* config = MSXConfig::Backend::createBackend("xml");
	
	try {
	  CommandLineParser::parse(config,argc,argv);
	  initializeSDL();

	  EmuTime zero;
	  config->initDeviceIterator();
	  MSXConfig::Device* d;
	  while ((d=config->getNextDevice()) != 0) {
	    std::cout << "<device>" << std::endl;
	    d->dump();
	    std::cout << "</device>" << std::endl << std::endl;
	    MSXDevice *device = deviceFactory::create(d, zero);
	    MSXMotherBoard::instance()->addDevice(device);
	    PRT_DEBUG ("Instantiated: " << d->getType());
	  }

	  // Start a new thread for event handling
	  Thread thread(EventDistributor::instance());
	  thread.start();

	  PRT_DEBUG ("starting MSX");
	  MSXMotherBoard::instance()->StartMSX();

	  // When we return we clean everything up
	  thread.stop();
	  MSXMotherBoard::instance()->DestroyMSX();
	} 
	catch (MSXException& e) {
		PRT_ERROR("Uncaught exception: " << e.desc);
	}
	exit (0);
}

