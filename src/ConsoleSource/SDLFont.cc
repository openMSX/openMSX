// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include "SDL_image.h"
#include "SDLFont.hh"
#include "openmsx.hh"
#include "MSXException.hh"


SDLFont::SDLFont(const std::string &bitmapName)
{
	// load the font bitmap
	SDL_Surface *tempSurface;
	if (!(tempSurface = IMG_Load(bitmapName.c_str())))
		throw MSXException("Can't load font");

	fontSurface = SDL_DisplayFormat(tempSurface);
	SDL_FreeSurface(tempSurface);

	charWidth  = fontSurface->w / 256;
	charHeight = fontSurface->h;

	// Set font as transparent. The assumption we'll go on is that the
	// first pixel of the font image will be the color we should treat
	// as transparent.
	byte r = ((byte*)fontSurface->pixels)[0];
	byte g = ((byte*)fontSurface->pixels)[1];
	byte b = ((byte*)fontSurface->pixels)[2];
	SDL_SetColorKey(fontSurface, SDL_SRCCOLORKEY | SDL_RLEACCEL, 
	                SDL_MapRGB(fontSurface->format, r, g, b));
}

SDLFont::~SDLFont()
{
	SDL_FreeSurface(fontSurface);
}

void SDLFont::drawText(const std::string &string, SDL_Surface *surface, int x, int y)
{
	// see how many characters can fit on the screen
	if (x > surface->w || y > surface->h)
		return;
	int characters = string.length();
	if (characters > (surface->w - x) / charWidth)
		characters = (surface->w - x) / charWidth;

	SDL_Rect DestRect;
	DestRect.x = x;
	DestRect.y = y;
	DestRect.w = charWidth;
	DestRect.h = charHeight;

	SDL_Rect SourceRect;
	SourceRect.y = 0;
	SourceRect.w = charWidth;
	SourceRect.h = charHeight;

	// Now draw it
	for (int loop=0; loop<characters; loop++) {
		SourceRect.x = string[loop] * charWidth;
		SDL_BlitSurface(fontSurface, &SourceRect, surface, &DestRect);
		DestRect.x += charWidth;
	}
}

int SDLFont::height()
{
	return charHeight;
}

int SDLFont::width()
{
	return charWidth;
}

