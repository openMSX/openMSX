// $Id$

#ifndef __SDLCONSOLE_HH__
#define __SDLCONSOLE_HH__

#include <memory>
#include <SDL.h>
#include "OSDConsoleRenderer.hh"

using std::auto_ptr;

namespace openmsx {

class Console;

class SDLConsole : public OSDConsoleRenderer
{
public:
	SDLConsole(Console& console_, SDL_Surface* screen);
	virtual ~SDLConsole();

	virtual bool loadFont(const string& filename);
	virtual bool loadBackground(const string& filename);
	virtual void drawConsole();
	virtual void updateConsole();

private:
	SDL_Surface* outputScreen;
	SDL_Surface* backgroundImage;

	auto_ptr<BackgroundSetting> backgroundSetting;
	auto_ptr<FontSetting> fontSetting;
	Console& console;

	SDL_Rect destRect;

	void updateConsoleRect();
	int zoomSurface(SDL_Surface* src, SDL_Surface* dst, bool smooth);
};

} // namespace openmsx

#endif
