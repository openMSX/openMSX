// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Adapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <cassert>
#include "Display.hh"
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
	Display::instance().addLayer(this);
}

SDLConsole::~SDLConsole()
{
}

void SDLConsole::updateConsoleRect()
{
	if (OSDConsoleRenderer::updateConsoleRect()) {
		try {
			loadBackground(backgroundName);
		} catch (MSXException& e) {
			// ignore
		}
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
				destW, destH, CONSOLE_ALPHA));
		} catch (MSXException& e) {
			// nothing
		}
	}
	if (backgroundImage.get()) {
		backgroundImage->draw(destX, destY, visibility);
	}

	int screenlines = destH / font->getHeight();
	for (int loop = 0; loop < screenlines; ++loop) {
		int num = loop + console.getScrollBack();
		font->drawText(console.getLine(num),
			destX + CHAR_BORDER,
			destY + destH - (1 + loop) * font->getHeight(),
			visibility);
	}

	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime) {
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		blink = !blink;
	}

	unsigned cursorX, cursorY;
	console.getCursorPosition(cursorX, cursorY);
	if ((cursorX != lastCursorX) || (cursorY != lastCursorY)) {
		blink = true; // force cursor
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE; // maximum time
		lastCursorX = cursorX;
		lastCursorY = cursorY;
	}
	if (console.getScrollBack() == 0) {
		if (blink) {
			font->drawText(string("_"),
				destX + CHAR_BORDER + cursorX * font->getWidth(),
				destY + destH - (font->getHeight() * (cursorY + 1)),
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
void SDLConsole::loadBackground(const string& filename)
{
	if (filename.empty()) {
		backgroundImage.reset();
		return;
	}
	backgroundImage.reset(new SDLImage(outputScreen, filename,
		destW, destH, CONSOLE_ALPHA));
	backgroundName = filename;
}

void SDLConsole::loadFont(const string& filename)
{
	File file(filename);
	font.reset(new SDLFont(&file, outputScreen));
}

unsigned SDLConsole::getScreenW() const
{
	return outputScreen->w;
}

unsigned SDLConsole::getScreenH() const
{
	return outputScreen->h;
}

} // namespace openmsx
