// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Adapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <cassert>
#include "SDLConsole.hh"
#include "Console.hh"
#include "SDLFont.hh"
#include "File.hh"
#include "SDLImage.hh"

using std::string;

namespace openmsx {

SDLConsole::SDLConsole(Console& console_, SDL_Surface* screen)
	: OSDConsoleRenderer(console_)
	, outputScreen(screen)
{
	blink = false;
	lastBlinkTime = 0;

	initConsole();
}

SDLConsole::~SDLConsole()
{
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
	byte visibility = getVisibility();
	if (!visibility) return;
	
	updateConsoleRect();

	// draw the background image if there is one
	if (!backgroundImage.get()) {
		// no background image, try to create an empty one
		try {
			backgroundImage.reset(new SDLImage(outputScreen,
				destRect.w, destRect.h, CONSOLE_ALPHA));
		} catch (MSXException& e) {
			// nothing
		}
	}
	if (backgroundImage.get()) {
		backgroundImage->draw(destRect.x, destRect.y, visibility);
	}

	int screenlines = destRect.h / font->getHeight();
	for (int loop = 0; loop < screenlines; ++loop) {
		int num = loop + console.getScrollBack();
		font->drawText(console.getLine(num),
			destRect.x + CHAR_BORDER,
			destRect.y + destRect.h - (1 + loop) * font->getHeight(),
			visibility);
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
				destRect.y + destRect.h - (font->getHeight() * (cursorY + 1)),
				visibility);
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
		backgroundImage.reset();
		return true;
	}
	try {
		backgroundImage.reset(new SDLImage(outputScreen, filename,
			destRect.w, destRect.h, CONSOLE_ALPHA));
		backgroundName = filename;
	} catch (MSXException& e) {
		return false;
	}
	return true;
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
