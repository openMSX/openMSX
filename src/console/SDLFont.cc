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


namespace openmsx {

const int NUM_CHRS = 256;
const int CHARS_PER_ROW = 16;
const int CHARS_PER_COL = NUM_CHRS / CHARS_PER_ROW;


SDLFont::SDLFont(File* file)
{
	// load the font bitmap
	SDL_Surface *image1;
	if (!(image1 = IMG_Load(file->getLocalName().c_str()))) {
		throw MSXException("Can't load font");
	}
	SDL_SetAlpha(image1, 0, 0);
	
	int width  = image1->w;
	int height = image1->h;

	SDL_Surface* image2 = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
		0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
	);
	if (image2 == NULL) {
		throw MSXException("Can't create font surface");
	}
	SDL_Rect area;
	area.x = 0;
	area.y = 0;
	area.w = width;
	area.h = height;
	SDL_BlitSurface(image1, &area, image2, &area);
	SDL_FreeSurface(image1);

	for (int i = 0; i < width * height; i++) {
		byte &r = ((byte*)image2->pixels)[4 * i + 0];
		byte &g = ((byte*)image2->pixels)[4 * i + 1];
		byte &b = ((byte*)image2->pixels)[4 * i + 2];
		byte  a = ((byte*)image2->pixels)[4 * i + 3];
		if (a > 100) {
			r = (r * a) / 255;
			g = (g * a) / 255;
			b = (b * a) / 255;
		} else {
			r = g = b = 0;
		}
	}

	// Set font as transparent. The assumption we'll go on is that the
	// first pixel of the font image will be the color we should treat
	// as transparent.
	byte r = ((byte*)image2->pixels)[0];
	byte g = ((byte*)image2->pixels)[1];
	byte b = ((byte*)image2->pixels)[2];
	
	fontSurface = SDL_DisplayFormat(image2);
	SDL_FreeSurface(image2);
	SDL_SetColorKey(fontSurface, SDL_SRCCOLORKEY | SDL_RLEACCEL, 
	                SDL_MapRGB(fontSurface->format, r, g, b));
	
	charWidth  = fontSurface->w / CHARS_PER_ROW;
	charHeight = fontSurface->h / CHARS_PER_COL;
}

SDLFont::~SDLFont()
{
	SDL_FreeSurface(fontSurface);
}

void SDLFont::setSurface(SDL_Surface* surface)
{
	drawSurface = surface;
}

void SDLFont::drawText(const string &string, int x, int y)
{
	// see how many characters can fit on the screen
	if ((drawSurface->w <= x) || (drawSurface->h <= y)) {
		return;
	}
	unsigned characters = string.length();
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
	for (unsigned loop = 0; loop < characters; loop++) {
		sourceRect.x = (string[loop] % CHARS_PER_ROW) * charWidth;
		sourceRect.y = (string[loop] / CHARS_PER_ROW) * charHeight;
		SDL_BlitSurface(fontSurface, &sourceRect, drawSurface, &destRect);
		destRect.x += charWidth;
	}
}

} // namespace openmsx
