// $Id$

#ifndef __SDLFONT_HH__
#define __SDLFONT_HH__

#include <SDL/SDL.h>
#include <string>


class SDLFont {
	public:
		SDLFont(const char *BitmapName);
		~SDLFont();

		void drawText(const std::string &string, SDL_Surface *surface, int x, int y);
		int height();
		int width();

	private:
		void setAlphaGL(int alpha);

		int charWidth;
		int charHeight;
	public: SDL_Surface *fontSurface; // temp hack, should be private
};

#endif
