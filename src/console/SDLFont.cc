// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include "SDLFont.hh"
#include "openmsx.hh"
#include "MSXException.hh"
#include "File.hh"
#include <SDL.h>
#include <SDL_image.h>


namespace openmsx {

const int NUM_CHRS = 256;
const int CHARS_PER_ROW = 16;
const int CHARS_PER_COL = NUM_CHRS / CHARS_PER_ROW;


SDLFont::SDLFont(File* file, SDL_Surface* surface)
	: outputScreen(surface)
{
	// load the font bitmap
	SDL_Surface* image1;
	if (!(image1 = IMG_Load(file->getLocalName().c_str()))) {
		throw MSXException("Can't load font");
	}
	fontSurface = SDL_DisplayFormatAlpha(image1);
	SDL_FreeSurface(image1);
	
	charWidth  = fontSurface->w / CHARS_PER_ROW;
	charHeight = fontSurface->h / CHARS_PER_COL;

	SDL_PixelFormat* format = fontSurface->format;
	workImage = SDL_CreateRGBSurface(SDL_SWSURFACE,
		charWidth, charHeight, format->BitsPerPixel,
		format->Rmask, format->Gmask, format->Bmask, 0);
	if (!workImage) {
		SDL_FreeSurface(fontSurface);
		throw MSXException("Can't load font");
	}
}

SDLFont::~SDLFont()
{
	SDL_FreeSurface(workImage);
	SDL_FreeSurface(fontSurface);
}

void SDLFont::drawText(const std::string& str, int x, int y, byte alpha)
{
	// see how many characters can fit on the screen
	if ((outputScreen->w <= x) || (outputScreen->h <= y)) {
		return;
	}
	unsigned characters = str.length();
	if (characters > ((outputScreen->w - x) / charWidth)) {
		characters = (outputScreen->w - x) / charWidth;
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
		sourceRect.x = (str[loop] % CHARS_PER_ROW) * charWidth;
		sourceRect.y = (str[loop] / CHARS_PER_ROW) * charHeight;
		if (alpha == 255) {
			SDL_BlitSurface(fontSurface, &sourceRect,
			                outputScreen, &destRect);
		} else {
			SDL_BlitSurface(outputScreen, &destRect,
			                workImage, NULL);
			SDL_BlitSurface(fontSurface, &sourceRect,
			                workImage, NULL);
			SDL_SetAlpha(workImage, SDL_SRCALPHA, alpha);
			SDL_BlitSurface(workImage, NULL,
			                outputScreen, &destRect);
		}
		destRect.x += charWidth;
	}
}

} // namespace openmsx
