// $Id$

/*
 *  openmsx - Emulate the MSX standard.
 *
 *  Copyright (C) 2001 David Heremans
 */

#include "config.h"
#include "MSXConfig.hh"
#include <SDL/SDL.h>
#include "Thread.hh"
#include "MSXMotherBoard.hh"
#include "DeviceFactory.hh"
#include "EventDistributor.hh"
#include "EmuTime.hh"
#include "CommandLineParser.hh"
#include "Icon.hh"
#include "CommandController.hh"
#include "KeyEventInserter.hh"
#include "MSXCPUInterface.hh"
#include "CliCommunicator.hh"


namespace openmsx {

void initializeSDL()
{
	Uint32 sdl_initval = SDL_INIT_VIDEO;
	if (DEBUGVAL) sdl_initval |= SDL_INIT_NOPARACHUTE; // dump core on segfault
	if (SDL_Init(sdl_initval) < 0) {
		throw FatalError(string("Couldn't init SDL: ") + SDL_GetError());
	}
	SDL_WM_SetCaption("openMSX " VERSION " [alpha]", 0);

	// Set icon
	static unsigned int iconRGBA[256];
	for (int i = 0; i < 256; i++) {
		iconRGBA[i] = iconColours[iconData[i]];
	}
	SDL_Surface *iconSurf = SDL_CreateRGBSurfaceFrom(
		iconRGBA, 16, 16, 32, 64,
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	SDL_SetColorKey(iconSurf, SDL_SRCCOLORKEY, 0);
	SDL_WM_SetIcon(iconSurf, NULL);
	SDL_FreeSurface(iconSurf);
}


int main(int argc, char **argv)
{
	try {
		CommandLineParser::instance()->parse(argc, argv);
		initializeSDL();

		// Initialise devices.
		EmuTime zero;
		MSXConfig* config = MSXConfig::instance();
		config->initDeviceIterator();
		Device* d;
		while ((d = config->getNextDevice()) != 0) {
			PRT_DEBUG("Instantiating: " << d->getType());
			MSXDevice *device = DeviceFactory::create(d, zero);
			MSXMotherBoard::instance()->addDevice(device);
		}
		// Register all postponed slots.
		MSXCPUInterface::instance()->registerPostSlots();

		// First execute auto commands.
		CommandController::instance()->autoCommands();

		// Schedule key insertions.
		// TODO move this somewhere else
		KeyEventInserter* keyEvents = new KeyEventInserter(zero);

		Thread communicatorThread(&CliCommunicator::instance());
		communicatorThread.start();
		
		// Start emulation thread.
		PRT_DEBUG("Starting MSX");
		MSXMotherBoard::instance()->run();

		communicatorThread.stop();
		delete keyEvents;

	} catch (FatalError& e) {
		cerr << "Fatal error: " << e.getMessage() << endl;
	} catch (MSXException& e) {
		cerr << "Uncaught exception: " << e.getMessage() << endl;
	} catch (...) {
		cerr << "Uncaught exception of unexpected type." << endl;
	}
	// Clean up.
	if (SDL_WasInit(SDL_INIT_EVERYTHING)) {
		SDL_Quit();
	}
	return 0;
}

} // namespace openmsx

// Enter the openMSX namespace.
int main(int argc, char **argv)
{
	return openmsx::main(argc, argv);
}
