// $Id$

/*
 *  openmsx - Emulate the MSX standard.
 *
 *  Copyright (C) 2001 David Heremans
 */

#define __OPENMSX_CC__

#include "config.h"
#include "MSXConfig.hh"
#include <SDL/SDL.h>
#include "Thread.hh"
#include "MSXMotherBoard.hh"
#include "DeviceFactory.hh"
#include "EventDistributor.hh"
#include "EmuTime.hh"
#include "CommandLineParser.hh"
#include "icon.nn"
#include "CommandController.hh"
#include "KeyEventInserter.hh"


void initializeSDL()
{
	Uint32 sdl_initval = SDL_INIT_VIDEO | SDL_INIT_EVENTTHREAD;
	if (DEBUGVAL) sdl_initval |= SDL_INIT_NOPARACHUTE; // dump copre on segfault
	if (SDL_Init(sdl_initval) < 0)
		PRT_ERROR("Couldn't init SDL: " << SDL_GetError());
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


int main(int argc, char **argv)
{
	// create configuration backend
	// for now there is only one, "xml" based
	MSXConfig::Backend* config = MSXConfig::Backend::createBackend("xml");
	try {
		CommandLineParser::instance()->parse(config, argc, argv);
		initializeSDL();

		EmuTime zero;
		config->initDeviceIterator();
		MSXConfig::Device* d;
		while ((d = config->getNextDevice()) != 0) {
			PRT_DEBUG ("Instantiating: " << d->getType());
			MSXDevice *device = DeviceFactory::create(d, zero);
			MSXMotherBoard::instance()->addDevice(device);
		}

		// Start a new thread for event handling
		Thread thread(EventDistributor::instance());
		thread.start();

		// First execute auto commands
		CommandController::instance()->autoCommands();

		//
		new KeyEventInserter();

		PRT_DEBUG ("Starting MSX");
		MSXMotherBoard::instance()->startMSX();

		// When we return we clean everything up
		thread.stop();
		MSXMotherBoard::instance()->destroyMSX();
		SDL_Quit();
	} 
	catch (MSXException& e) {
		PRT_ERROR("Uncaught exception: " << e.desc);
	}
}

