// $Id$

#ifndef __SDLFONT_HH__
#define __SDLFONT_HH__

#include <string>
#include "SDL/SDL.h"


class SDLFont
{
	public:
		SDLFont(const std::string &bitmapName);
		~SDLFont();

		void drawText(const std::string &string, SDL_Surface *surface, int x, int y);
		int height();
		int width();

	private:
		int charWidth;
		int charHeight;
		SDL_Surface *fontSurface;
};

#endif
