// $Id$

/*
 *  openmsx - Emulate the MSX standard.
 *
 *  Copyright (C) 2001 David Heremans
 */

#include <iostream>
#include <SDL/SDL.h>
#include "config.h"
#include "MSXMotherBoard.hh"
#include "CommandLineParser.hh"
#include "Icon.hh"

using std::cerr;
using std::endl;

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
	int err = 0;
	try {
		initializeSDL();
		CommandLineParser::ParseStatus parseStatus =
			CommandLineParser::instance()->parse(argc, argv);
		if (parseStatus == CommandLineParser::OK) {
			// Start emulation thread.
			MSXMotherBoard::instance()->run();
		}
	} catch (FatalError& e) {
		cerr << "Fatal error: " << e.getMessage() << endl;
		err = 1;
	} catch (MSXException& e) {
		cerr << "Uncaught exception: " << e.getMessage() << endl;
		err = 1;
	} catch (...) {
		cerr << "Uncaught exception of unexpected type." << endl;
		err = 1;
	}
	// Clean up.
	if (SDL_WasInit(SDL_INIT_EVERYTHING)) {
		SDL_Quit();
	}
	return err;
}

} // namespace openmsx

// Enter the openMSX namespace.
int main(int argc, char **argv)
{
	return openmsx::main(argc, argv);
}
