// $Id$

/*
 *  openmsx - Emulate the MSX standard.
 *
 *  Copyright (C) 2001 David Heremans
 */

#include <memory> // for auto_ptr
#include <iostream>
#include <SDL/SDL.h>
#include <exception>
#include "config.h"
#include "MSXMotherBoard.hh"
#include "CommandLineParser.hh"
#include "Icon.hh"
#include "CliCommInput.hh"
#include "HotKey.hh"

using std::auto_ptr;
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

void unexpectedExceptionHandler()
{
	cerr << "Unexpected exception." << endl;
}

int main(int argc, char **argv)
{
	std::set_unexpected(unexpectedExceptionHandler);
	
	int err = 0;
	try {
		initializeSDL();
		CommandLineParser::ParseStatus parseStatus =
			CommandLineParser::instance()->parse(argc, argv);
		if (parseStatus != CommandLineParser::EXIT) {
			auto_ptr<CliCommInput> cliCommInput;
			if (parseStatus) {
				cliCommInput.reset(new CliCommInput());
			}
			HotKey hotkey;
			MSXMotherBoard::instance()->run(
				parseStatus == CommandLineParser::RUN);
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
