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
#include "FileOperations.hh"


namespace openmsx {

void initializeSDL()
{
	Uint32 sdl_initval = SDL_INIT_VIDEO;
	if (DEBUGVAL) sdl_initval |= SDL_INIT_NOPARACHUTE; // dump core on segfault
	if (SDL_Init(sdl_initval) < 0) {
		PRT_ERROR("Couldn't init SDL: " << SDL_GetError());
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


inline int main(int argc, char **argv)
{
	if (FileOperations::setSysDir())
	{	fprintf(stderr,"Cannot detect openMSX directory.\n");
		fflush(stderr);
		return	2;
	}

	if (FileOperations::setUsrDir())
	{	fprintf(stderr,"Cannot get user-directory.\n");
		fflush(stderr);
		return	2;
	}
#if	defined(__WIN32__)
	char	**nargv;
	int	i;
	if ((nargv=(char**)malloc(argc * sizeof(*argv)))==NULL) {
		fprintf(stderr,"Cannot allocate argument buffer.\n");
		fflush(stderr);
		return	2;
	}
	for (i=0;i<argc;i++)
		if ((nargv[i]=(char*)malloc(strlen(argv[i])+1))!=NULL)
			strcpy(nargv[i], FileOperations::getConventionalPath(argv[i]).c_str());
	for (i=0;i<argc;i++)
		if (nargv[i]==NULL) {
			for (i=0;i<argc;i++)
				if (nargv[i]!=NULL)
					free(nargv[i]);
			free(nargv);
			fprintf(stderr,"Cannot allocate arguments buffer.\n");
			fflush(stderr);
			return	2;
		}

	try {
		CommandLineParser::instance()->parse(argc, nargv);
#else

	try {
		CommandLineParser::instance()->parse(argc, argv);
#endif
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

		// Start emulation thread.
		PRT_DEBUG("Starting MSX");
		MSXMotherBoard::instance()->run();

		delete keyEvents;

		// Clean up.
		SDL_Quit();

#if	defined(__WIN32__)
		for (i=0;i<argc;i++)
			if (nargv[i]!=NULL)
				free(nargv[i]);
		free(nargv);
#endif
		return 0;

	} catch (MSXException &e) {
		PRT_ERROR("Uncaught exception: " << e.getMessage());
	} catch (...) {
		PRT_ERROR("Uncaught exception of unexpected type.");
	}
}

} // namespace openmsx

// Enter the openMSX namespace.
int main(int argc, char **argv)
{
	return	openmsx::main(argc, argv);
}
