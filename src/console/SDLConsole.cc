// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Adapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <cassert>
#include <SDL_image.h>
#include "SDLConsole.hh"
#include "SDLFont.hh"
#include "File.hh"
#include "CommandConsole.hh"
#include "SDLImage.hh"

namespace openmsx {

SDLConsole::SDLConsole(Console& console_, SDL_Surface* screen)
	: OSDConsoleRenderer(console_)
{
	blink = false;
	lastBlinkTime = 0;
	backgroundImage = 0;
	outputScreen = screen;

	initConsole();
}

SDLConsole::~SDLConsole()
{
	if (backgroundImage) {
		SDL_FreeSurface(backgroundImage);
	}
}

void SDLConsole::updateConsoleRect()
{
	SDL_Rect rect;
	OSDConsoleRenderer::updateConsoleRect(rect);
	if ((destRect.h != rect.h) || (destRect.w != rect.w) ||
	    (destRect.x != rect.x) || (destRect.y != rect.y)) {
		destRect.h = rect.h;
		destRect.w = rect.w;
		destRect.x = rect.x;
		destRect.y = rect.y;
		loadBackground(backgroundName);
	}
}

void SDLConsole::paint()
{
	updateConsoleRect();

	// draw the background image if there is one
	if (!backgroundImage) {
		SDL_PixelFormat* format = outputScreen->format;
		backgroundImage = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
			destRect.w, destRect.h, format->BitsPerPixel,
			format->Rmask, format->Gmask, format->Bmask, format->Amask);
		SDL_SetAlpha(backgroundImage, SDL_SRCALPHA, CONSOLE_ALPHA);
	}
	SDL_BlitSurface(backgroundImage, NULL, outputScreen, &destRect);

	int screenlines = destRect.h / font->getHeight();
	for (int loop = 0; loop < screenlines; ++loop) {
		int num = loop + console.getScrollBack();
		font->drawText(console.getLine(num),
			destRect.x + CHAR_BORDER,
			destRect.y + destRect.h - (1 + loop) * font->getHeight());
	}

	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime) {
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		blink = !blink;
	}

	unsigned cursorX;
	unsigned cursorY;
	console.getCursorPosition(cursorX, cursorY);
	if (cursorX != lastCursorPosition){
		blink = true; // force cursor
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE; // maximum time
		lastCursorPosition = cursorX;
	}
	if (console.getScrollBack() == 0) {
		if (blink) {
			font->drawText(string("_"),
				destRect.x + CHAR_BORDER + cursorX * font->getWidth(),
				destRect.y + destRect.h - (font->getHeight() * (cursorY + 1)));
		}
	}
}

const string& SDLConsole::getName()
{
	static const string NAME = "SDLConsole";
	return NAME;
}

// Adds background image to the console
bool SDLConsole::loadBackground(const string& filename)
{
	if (filename.empty()) {
		if (backgroundImage) {
			SDL_FreeSurface(backgroundImage);
			backgroundImage = NULL;
		}
		return true;
	}
	SDL_Surface* pictureSurface = SDLImage::loadImage(
		filename, destRect.w, destRect.h, CONSOLE_ALPHA);
	if (pictureSurface) {
		if (backgroundImage) {
			SDL_FreeSurface(backgroundImage);
		}
		backgroundImage = pictureSurface;
		backgroundName = filename;
	}
	return pictureSurface;
}

bool SDLConsole::loadFont(const string& filename)
{
	if (filename.empty()) {
		return false;
	}
	try {
		File file(filename);
		font.reset(new SDLFont(&file, outputScreen));
	} catch (MSXException& e) {
		return false;
	}
	return true;
}

} // namespace openmsx
