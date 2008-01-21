// $Id$

#ifndef SDLFONT_HH
#define SDLFONT_HH

#include "Font.hh"
#include "noncopyable.hh"

class SDL_Surface;

namespace openmsx {

class OutputSurface;

class SDLFont : public Font, private noncopyable
{
public:
	SDLFont(const std::string& filename, OutputSurface& output);
	virtual ~SDLFont();

	virtual void drawText(const std::string& str, int x, int y, byte alpha);

private:
	OutputSurface& output;
	SDL_Surface* fontSurface;
	SDL_Surface* workImage;
};

} // namespace openmsx

#endif
