// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#ifdef HAVE_SDL_IMAGE_H
#include "SDL_image.h"
#else
#include "SDL/SDL_image.h"
#endif

#include "SDLFont.hh"
#include "openmsx.hh"
#include "MSXException.hh"
#include "MSXConfig.hh"
#include "File.hh"


const int NUM_CHRS = 256;
const int CHARS_PER_ROW = 16;
const int CHARS_PER_COL = NUM_CHRS / CHARS_PER_ROW;


SDLFont::SDLFont(Config *config)
{
	const std::string &fontName = config->getParameter("font");
	File file(config->getContext(), fontName);

	// load the font bitmap
	SDL_Surface *tempSurface;
	if (!(tempSurface = IMG_Load(file.getLocalName().c_str())))
		throw MSXException("Can't load font");

	fontSurface = SDL_DisplayFormat(tempSurface);
	SDL_FreeSurface(tempSurface);

	charWidth  = fontSurface->w / CHARS_PER_ROW;
	charHeight = fontSurface->h / CHARS_PER_COL;

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

void SDLFont::setSurface(SDL_Surface* surface)
{
	drawSurface = surface;
}

void SDLFont::drawText(const std::string &string,
                       int x, int y)
{
	// see how many characters can fit on the screen
	if ((drawSurface->w <= x) || (drawSurface->h <= y)) {
		return;
	}
	int characters = string.length();
	if (characters > ((drawSurface->w - x) / charWidth)) {
		characters = (drawSurface->w - x) / charWidth;
	}
	SDL_Rect destRect;
	destRect.x = x;
	destRect.y = y;
	destRect.w = charWidth;
	destRect.h = charHeight;

	SDL_Rect sourceRect;
	sourceRect.w = charWidth;
	sourceRect.h = charHeight;

	// Now draw it
	for (int loop = 0; loop < characters; loop++) {
		sourceRect.x = (string[loop] % CHARS_PER_ROW) * charWidth;
		sourceRect.y = (string[loop] / CHARS_PER_ROW) * charHeight;
		SDL_BlitSurface(fontSurface, &sourceRect, drawSurface, &destRect);
		destRect.x += charWidth;
	}
}
