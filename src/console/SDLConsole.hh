// $Id$

#ifndef __SDLCONSOLE_HH__
#define __SDLCONSOLE_HH__

#include <SDL.h>
#include "OSDConsoleRenderer.hh"

namespace openmsx {

class SDLConsole : public OSDConsoleRenderer
{
public:
	SDLConsole(Console& console_, SDL_Surface* screen);
	virtual ~SDLConsole();

	virtual bool loadFont(const string& filename);
	virtual bool loadBackground(const string& filename);

	virtual void paint();
	virtual const string& getName();

private:
	void updateConsoleRect();
	int zoomSurface(SDL_Surface* src, SDL_Surface* dst, bool smooth);
	
	SDL_Surface* outputScreen;
	SDL_Surface* backgroundImage;
	string backgroundName;
};

} // namespace openmsx

#endif
