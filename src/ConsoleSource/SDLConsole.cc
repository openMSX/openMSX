// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Addapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <cassert>

#ifdef HAVE_SDL_IMAGE_H
#include "SDL_image.h"
#else
#include "SDL/SDL_image.h"
#endif

#include "SDLConsole.hh"
#include "SDLFont.hh"
#include "MSXConfig.hh"
#include "File.hh"


SDLConsole::SDLConsole(SDL_Surface *screen)
{
	if (fontName.empty()) {
		font = NULL;
		return;
	}

	blink = false;
	lastBlinkTime = 0;
	
	outputScreen = screen;
	backgroundImage = NULL;
	consoleSurface  = NULL;
	inputBackground = NULL;
	consoleAlpha = SDL_ALPHA_OPAQUE;
	
	File file(context, fontName);
	font = new SDLFont(&file);
	
	SDL_Rect rect;
	rect.x = (screen->w / 32);
	rect.w = (screen->w / 32) * 30;
	rect.y = (screen->h / 15) * 9;
	rect.h = (screen->h / 15) * 6;
	resize(rect);
	
	loadBackground();
	
	alpha(180);
}

SDLConsole::~SDLConsole()
{
	PRT_DEBUG("Destroying SDLConsole");
	delete font;
}


// Updates the console buffer
void SDLConsole::updateConsole()
{
	SDL_FillRect(consoleSurface, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));

	// draw the background image if there is one
	if (backgroundImage) {
		SDL_Rect destRect;
		destRect.x = 0;
		destRect.y = 0;
		destRect.w = backgroundImage->w;
		destRect.h = backgroundImage->h;
		SDL_BlitSurface(backgroundImage, NULL, consoleSurface, &destRect);
	}

	int screenlines = consoleSurface->h / font->getHeight();
	for (int loop=0; loop<screenlines; loop++) {
		int num = loop+consoleScrollBack;
		if (num < lines.size())
			font->drawText(lines[num], CHAR_BORDER,
			       consoleSurface->h - (1+loop)*font->getHeight());
	}
}

// Draws the console buffer to the screen
void SDLConsole::drawConsole()
{
	if (!isVisible) return;
	
	drawCursor();

	// Setup the rect the console is being blitted into based on the output screen
	SDL_Rect destRect;
	destRect.x = dispX;
	destRect.y = dispY;
	destRect.w = consoleSurface->w;
	destRect.h = consoleSurface->h;
	SDL_BlitSurface(consoleSurface, NULL, outputScreen, &destRect);
}



// Draws the command line the user is typing in to the screen
void SDLConsole::drawCursor()
{
	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime) {
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		blink = !blink;
		if (consoleScrollBack > 0)
			return;
		int cursorLocation = lines[0].length();
		if (blink) {
			// Print cursor if there is enough room
			font->drawText(std::string("_"),
				      CHAR_BORDER + cursorLocation * font->getWidth(),
				      consoleSurface->h - font->getHeight());
		} else {
			// Remove cursor
			SDL_Rect rect;
			rect.x = cursorLocation * font->getWidth() + CHAR_BORDER;
			rect.y = consoleSurface->h - font->getHeight();
			rect.w = font->getWidth();
			rect.h = font->getHeight();
			SDL_FillRect(consoleSurface, &rect, 
				     SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));
			if (backgroundImage) {
				// draw the background image if applicable
				SDL_Rect rect2;
				rect2.x = cursorLocation * font->getWidth() + CHAR_BORDER;
				rect.x = rect2.x;
				rect2.y = consoleSurface->h - font->getHeight();
				rect.y = rect2.y;
				rect2.w = rect.w = font->getWidth();
				rect2.h = rect.h = font->getHeight();
				SDL_BlitSurface(backgroundImage, &rect, consoleSurface, &rect2);
			}
		}
	}
}

// Sets the alpha level of the console, 0 turns off alpha blending
void SDLConsole::alpha(unsigned char newAlpha)
{
	// store alpha as state!
	consoleAlpha = newAlpha;
	if (consoleAlpha == 0)
		SDL_SetAlpha(consoleSurface, 0,            consoleAlpha);
	else
		SDL_SetAlpha(consoleSurface, SDL_SRCALPHA, consoleAlpha);
	updateConsole();
}

// Adds  background image to the console
//  x and y are based on console x and y
void SDLConsole::loadBackground()
{
	if (!backgroundName.empty()) {
		File file(context, backgroundName);
		
		SDL_Surface *temp;
		if (!(temp = IMG_Load(file.getLocalName().c_str())))
			return;
		if (backgroundImage)
			SDL_FreeSurface(backgroundImage);
		backgroundImage = SDL_DisplayFormat(temp);
		SDL_FreeSurface(temp);
		reloadBackground();
	}
}

// resizes the console, has to reset alot of stuff
void SDLConsole::resize(SDL_Rect rect)
{
	// make sure that the size of the console is valid
	assert (!(rect.w > outputScreen->w || rect.w < font->getWidth() * 32));
	assert (!(rect.h > outputScreen->h || rect.h < font->getHeight()));

	SDL_Surface *temp1 = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, 
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	SDL_Surface *temp2 = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, font->getHeight(),
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if (temp1 == NULL || temp2 == NULL)
		return;
	if (consoleSurface)  SDL_FreeSurface(consoleSurface);
	if (inputBackground) SDL_FreeSurface(inputBackground);
	consoleSurface = SDL_DisplayFormat(temp1);
	font->setSurface(consoleSurface);
	inputBackground = SDL_DisplayFormat(temp2);
	SDL_FreeSurface(temp1);
	SDL_FreeSurface(temp2);
	SDL_FillRect(consoleSurface, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));
	
	consoleScrollBack = 0;	// dependent on previous size
	position(rect.x, rect.y);
	reloadBackground();
}

// takes a new x and y of the top left of the console window
void SDLConsole::position(int x, int y)
{
	assert (!(x<0 || x > outputScreen->w - consoleSurface->w));
	assert (!(y<0 || y > outputScreen->h - consoleSurface->h));
	dispX = x;
	dispY = y;
}

void SDLConsole::reloadBackground()
{
	SDL_FillRect(inputBackground, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, SDL_ALPHA_OPAQUE));
	if (backgroundImage) {
		SDL_Rect src;
			src.x = 0;
			src.y = consoleSurface->h - font->getHeight();
			src.w = backgroundImage->w;
			src.h = inputBackground->h;
		SDL_Rect dest;
			dest.x = 0;
			dest.y = 0;
			dest.w = backgroundImage->w;
			dest.h = font->getHeight();
		SDL_BlitSurface(backgroundImage, &src, inputBackground, &dest);
	}
}
