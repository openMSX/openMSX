// $Id$

#ifndef __SDLCONSOLE_HH__
#define __SDLCONSOLE_HH__

#include "OSDConsoleRenderer.hh"
#include <SDL.h>
#include <memory>

namespace openmsx {

class SDLImage;

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
	
	SDL_Surface* outputScreen;
	std::auto_ptr<SDLImage> backgroundImage;
	string backgroundName;
};

} // namespace openmsx

#endif
