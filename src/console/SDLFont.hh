// $Id$

#ifndef __SDLFONT_HH__
#define __SDLFONT_HH__

#include "Font.hh"

struct SDL_Surface;

namespace openmsx {

class File;

class SDLFont : public Font
{
public:
	SDLFont(File* file, SDL_Surface* surface);
	virtual ~SDLFont();

	virtual void drawText(const std::string& str, int x, int y, byte alpha);

private:
	SDL_Surface* outputScreen;
	SDL_Surface* fontSurface;
	SDL_Surface* workImage;
};

} // namespace openmsx

#endif // __SDLFONT_HH__
