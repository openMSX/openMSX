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
	
	// create a list of used config files
	std::list<std::string> configfiles;
	if (argc<2) {
		configfiles.push_back(std::string("msxconfig.xml"));
		PRT_INFO ("Using msxconfig.xml as default configuration file.");
	} else {
		for (int i = 1; i < argc; i++)
		{
		configfiles.push_back(std::string(argv[i]));
		}
	}

	try {
		// Load all config files in memory
		for (std::list<std::string>::const_iterator i = configfiles.begin(); i != configfiles.end(); i++)
		{
			config->loadFile(*i);
		}
		
		initializeSDL();
	
		EmuTime zero;

		config->initDeviceIterator();
		MSXConfig::Device* d=0;
		while ((d=config->getNextDevice()) != 0)
		{
			//std::cout << "<device>" << std::endl;
			//d->dump();
			//std::cout << "</device>" << std::endl << std::endl;
			MSXDevice *device = deviceFactory::create(d, zero);
			MSXMotherBoard::instance()->addDevice(device);
			PRT_DEBUG ("Instantiated:" << d->getType());
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


