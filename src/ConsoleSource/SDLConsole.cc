// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Addapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <cassert>
#include "../openmsx.hh"
#include "SDLConsole.hh"
#include "CommandController.hh"
#include "HotKey.hh"
#include "ConsoleManager.hh"


SDLConsole::SDLConsole(SDL_Surface *screen) :
	consoleCmd(this)
{
	isVisible = false;
	blink = false;
	lastBlinkTime = 0;
	
	outputScreen = screen;
	backgroundImage = NULL;
	consoleSurface  = NULL;
	inputBackground = NULL;
	consoleAlpha = SDL_ALPHA_OPAQUE;
	font = new SDLFont("ConsoleFont.bmp");	// TODO check for error

	SDL_Rect rect;
	rect.x = 20;
	rect.y = 288;
	rect.w = 600;
	rect.h = 192;
	resize(rect);

	alpha(180);
	SDL_EnableUNICODE(1);

	ConsoleManager::instance()->registerConsole(this);
	EventDistributor::instance()->registerAsyncListener(SDL_KEYDOWN, this);
	CommandController::instance()->registerCommand(consoleCmd, "console");
	HotKey::instance()->registerHotKeyCommand(SDLK_F10, "console");
}

SDLConsole::~SDLConsole()
{
	PRT_DEBUG("Destroying a SDLConsole object");
	
	delete font;
}


// Takes keys from the keyboard and inputs them to the console
void SDLConsole::signalEvent(SDL_Event &event)
{
	if (!isVisible) return;
	
	switch (event.key.keysym.sym) {
		case SDLK_PAGEUP:
			scrollUp();
			break;
		case SDLK_PAGEDOWN:
			scrollDown();
			break;
		case SDLK_UP:
			prevCommand();
			break;
		case SDLK_DOWN:
			nextCommand();
			break;
		case SDLK_BACKSPACE:
			backspace();
			break;
		case SDLK_TAB:
			tabCompletion();
			break;
		case SDLK_RETURN:
			commandExecute();
			break;
		default:
			if (event.key.keysym.unicode)
				normalKey((char)event.key.keysym.unicode);
	}
	updateConsole();
}

/* setAlphaGL() -- sets the alpha channel of an SDL_Surface to the
 * specified value.  Preconditions: the surface in question is RGBA.
 * 0 <= a <= 255, where 0 is transparent and 255 is opaque. */
void SDLConsole::setAlphaGL(SDL_Surface *s, int alpha)
{
	// clamp alpha value to 0...255
	if (alpha < SDL_ALPHA_TRANSPARENT)
		alpha = SDL_ALPHA_TRANSPARENT;
	else if (alpha > SDL_ALPHA_OPAQUE)
		alpha = SDL_ALPHA_OPAQUE;

	// loop over alpha channels of each pixel, setting them appropriately
	Uint8 *last;
	Uint8 *pix;
	int numpixels;
	int w = s->w;
	int h = s->h;
	SDL_PixelFormat *format = s->format;
	switch (format->BytesPerPixel) {
	case 2:
		// 16-bit SDL surfaces do not support alpha-blending under OpenGL
		break;
	case 4:
		// we can do this very quickly in 32-bit mode.  24-bit is more
		// difficult.  And since 24-bit mode is reall the same as 32-bit,
		// so it usually ends up taking this route too.  Win!  Unroll loops
		// and use pointer arithmetic for extra speed.
		numpixels = h * w * 4;
		pix = (Uint8 *) (s->pixels);
		last = pix + numpixels;
		if (numpixels & 0x7 == 0)
			for (Uint8 *pixel = pix+3; pixel<last; pixel+=32)
				*pixel      = *(pixel+4)  = *(pixel+8)  = *(pixel+12) =
				*(pixel+16) = *(pixel+20) = *(pixel+24) = *(pixel+28) = alpha;
		else
			for (Uint8 *pixel = pix+3; pixel<last; pixel+=4)
				*pixel = alpha;
		break;
	default:
		// we have no choice but to do this slowly.  <sigh>
		Uint8 r, g, b, a;
		for (int y=0; y<h; ++y) {
			for (int x=0; x<w; ++x) {
				// Lock the surface for direct access to the pixels
				if (SDL_MUSTLOCK(s) && SDL_LockSurface(s) < 0) {
					// Can't lock surface
					return;
				}
				Uint32 pixel = getPixel(s, x, y);
				SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
				pixel = SDL_MapRGBA(format, r, g, b, alpha);
				SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
				putPixel(s, x, y, pixel);
				// unlock surface again
				if (SDL_MUSTLOCK(s))
					SDL_UnlockSurface(s);
			}
		}
		break;
	}
}


// Updates the console buffer
void SDLConsole::updateConsole()
{
	SDL_FillRect(consoleSurface, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));
	if (outputScreen->flags & SDL_OPENGLBLIT)
		SDL_SetAlpha(consoleSurface, 0, SDL_ALPHA_OPAQUE);

	// draw the background image if there is one
	if (backgroundImage) {
		SDL_Rect destRect;
		destRect.x = backX;
		destRect.y = backY;
		destRect.w = backgroundImage->w;
		destRect.h = backgroundImage->h;
		SDL_BlitSurface(backgroundImage, NULL, consoleSurface, &destRect);
	}

	// Draw the text from the back buffers, calculate in the scrollback from the user
	// this is a normal SDL software-mode blit, so we need to temporarily set the ColorKey
	// for the font, and then clear it when we're done.
	if ((outputScreen->flags & SDL_OPENGLBLIT) && (outputScreen->format->BytesPerPixel > 2)) {
		Uint32 *pix = (Uint32 *) (font->fontSurface->pixels);
		SDL_SetColorKey(font->fontSurface, SDL_SRCCOLORKEY, *pix);
	}
	int screenlines = consoleSurface->h / font->height();
	for (int loop=0; loop<screenlines; loop++) {
		int num = loop+consoleScrollBack;
		if (num < lines.size())
			font->drawText(lines[num], consoleSurface, CHAR_BORDER,
			               consoleSurface->h - (1+loop)*font->height());
	}
	if (outputScreen->flags & SDL_OPENGLBLIT)
		SDL_SetColorKey(font->fontSurface, 0, 0);
}

// Draws the console buffer to the screen
void SDLConsole::drawConsole()
{
	if (!isVisible) return;
	
	drawCursor();

	// before drawing, make sure the alpha channel of the console surface is set
	// properly.  (sigh) I wish we didn't have to do this every frame...
	if (outputScreen->flags & SDL_OPENGLBLIT)
		setAlphaGL(consoleSurface, consoleAlpha);

	// Setup the rect the console is being blitted into based on the output screen
	SDL_Rect destRect;
	destRect.x = dispX;
	destRect.y = dispY;
	destRect.w = consoleSurface->w;
	destRect.h = consoleSurface->h;
	SDL_BlitSurface(consoleSurface, NULL, outputScreen, &destRect);

	if (outputScreen->flags & SDL_OPENGLBLIT)
		SDL_UpdateRects(outputScreen, 1, &destRect);
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
			if (outputScreen->flags & SDL_OPENGLBLIT) {
				Uint32 *pix = (Uint32 *) (font->fontSurface->pixels);
				SDL_SetColorKey(font->fontSurface, SDL_SRCCOLORKEY, *pix);
			}
			font->drawText(std::string("_"), consoleSurface, 
				      CHAR_BORDER + cursorLocation * font->width(),
				      consoleSurface->h - font->height());
			if (outputScreen->flags & SDL_OPENGLBLIT)
				SDL_SetColorKey(font->fontSurface, 0, 0);
		} else {
			// Remove cursor
			SDL_Rect rect;
			rect.x = cursorLocation * font->width() + CHAR_BORDER;
			rect.y = consoleSurface->h - font->height();
			rect.w = font->width();
			rect.h = font->height();
			SDL_FillRect(consoleSurface, &rect, 
				     SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));
			if (backgroundImage) {
				// draw the background image if applicable
				SDL_Rect rect2;
				rect2.x = cursorLocation * font->width() + CHAR_BORDER;
				rect.x = rect2.x - backX;
				rect2.y = consoleSurface->h - font->height();
				rect.y = rect2.y - backY;
				rect2.w = rect.w = font->width();
				rect2.h = rect.h = font->height();
				SDL_BlitSurface(backgroundImage, &rect, consoleSurface, &rect2);
			}
		}
	}
}

// Sets the alpha level of the console, 0 turns off alpha blending
void SDLConsole::alpha(unsigned char alpha)
{
	// store alpha as state!
	consoleAlpha = alpha;

	if ((outputScreen->flags & SDL_OPENGLBLIT) == 0)
		if (alpha == 0)
			SDL_SetAlpha(consoleSurface, 0, alpha);
		else
			SDL_SetAlpha(consoleSurface, SDL_SRCALPHA, alpha);
	updateConsole();
}

// Adds  background image to the console
//  x and y are based on console x and y
void SDLConsole::background(const char *image, int x, int y)
{
	SDL_Surface *temp = NULL;
	if (image) 
		temp = SDL_LoadBMP(image);
	if (backgroundImage) {
		SDL_FreeSurface(backgroundImage);
		backgroundImage = NULL;
	}
	if (temp) {
		backgroundImage = SDL_DisplayFormat(temp);
		SDL_FreeSurface(temp);
	}
	backX = x;
	backY = y;
	reloadBackground();
}

// resizes the console, has to reset alot of stuff
void SDLConsole::resize(SDL_Rect rect)
{
	// make sure that the size of the console is valid
	assert (!(rect.w > outputScreen->w || rect.w < font->width() * 32));
	assert (!(rect.h > outputScreen->h || rect.h < font->height()));

	SDL_Surface *temp1 = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, 
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	SDL_Surface *temp2 = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, font->height(), 
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if (temp1 == NULL || temp2 == NULL)
		return;
	if (consoleSurface)  SDL_FreeSurface(consoleSurface);
	if (inputBackground) SDL_FreeSurface(inputBackground);
	consoleSurface = SDL_DisplayFormat(temp1);
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
			src.y = consoleSurface->h - font->height() - backY;
			src.w = backgroundImage->w;
			src.h = inputBackground->h;
		SDL_Rect dest;
			dest.x = backX;
			dest.y = 0;
			dest.w = backgroundImage->w;
			dest.h = font->height();
		SDL_BlitSurface(backgroundImage, &src, inputBackground, &dest);
	}
}

// Return the pixel value at (x, y)
// NOTE: The surface must be locked before calling this!
Uint32 SDLConsole::getPixel(SDL_Surface *surface, int x, int y)
{
	int bpp = surface->format->BytesPerPixel;
	// Here p is the address to the pixel we want to retrieve
	Uint8 *p = (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;
	switch (bpp) {
		case 1:
			return *p;
		case 2:
			return *(Uint16 *) p;
		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return p[0] << 16 | p[1] << 8 | p[2];
			else
				return p[0] | p[1] << 8 | p[2] << 16;
		case 4:
			return *(Uint32 *) p;
		default:
			return 0;	// shouldn't happen, but avoids warnings
	}
}

// Set the pixel at (x, y) to the given value
// NOTE: The surface must be locked before calling this!
void SDLConsole::putPixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
	int bpp = surface->format->BytesPerPixel;
	// Here p is the address to the pixel we want to set
	Uint8 *p = (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;
	switch (bpp) {
		case 1:
			*p = pixel;
			break;
		case 2:
			*(Uint16 *) p = pixel;
			break;
		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				p[0] = (pixel >> 16) & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = pixel & 0xff;
			} else {
				p[0] = pixel & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = (pixel >> 16) & 0xff;
			}
			break;
		case 4:
			*(Uint32 *) p = pixel;
			break;
		default:
			break;
	}
}


// Console command
SDLConsole::ConsoleCmd::ConsoleCmd(SDLConsole *cons)
{
	console = cons;
}

void SDLConsole::ConsoleCmd::execute(const std::vector<std::string> &tokens)
{
	switch (tokens.size()) {
	case 1:
		console->isVisible = !console->isVisible;
		break;
	case 2:
		if (tokens[1] == "on") {
			console->isVisible = true;
			break;
		}
		if (tokens[1] == "off") {
			console->isVisible = false;
			break;
		}
	default:
		ConsoleManager::instance()->print("Syntax error");
	}
}
void SDLConsole::ConsoleCmd::help   (const std::vector<std::string> &tokens)
{
	ConsoleManager::instance()->print("This command turns console display on/off");
	ConsoleManager::instance()->print(" console:     toggle console display");
	ConsoleManager::instance()->print(" console on:  show console display");
	ConsoleManager::instance()->print(" console off: remove console display");
} 
