// $Id$

#ifndef __SDLFONT_HH__
#define __SDLFONT_HH__

#include "SDL/SDL.h"
#include "Font.hh"

class Config;


class SDLFont : public Font
{
	public:
		SDLFont(Config *config);
		virtual ~SDLFont();

		void setSurface(SDL_Surface *surface);
		virtual void drawText(const std::string &string,
		                      int x, int y);

	private:
		SDL_Surface *fontSurface;
		SDL_Surface *drawSurface;
};

#endif
