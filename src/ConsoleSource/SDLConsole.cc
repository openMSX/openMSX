// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Addapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <cassert>
#include "../openmsx.hh"
#include "SDLConsole.hh"


SDLConsole::SDLConsole()
{
	isVisible = false;
	lastBlinkTime = 0;
}

SDLConsole::~SDLConsole()
{
	PRT_DEBUG("Destroying a SDLConsole object");
	
	delete font;
}

SDLConsole *SDLConsole::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new SDLConsole();
	}
	return (SDLConsole*)oneInstance;
}


void SDLConsole::hookUpSDLConsole(SDL_Surface *screen)
{
	SDL_Rect ConRect;
	ConRect.x = 20;
	ConRect.y = 300;
	ConRect.w = 600;
	ConRect.h = 180;
	init("ConsoleFont.bmp", screen, ConRect);
	alpha(200);
	SDL_EnableUNICODE(1);
	EventDistributor::instance()->registerAsyncListener(SDL_KEYDOWN, this);
	HotKey::instance()->registerAsyncHotKey(SDLK_F10, this);
}

// Note: this runs in a different thread
void SDLConsole::signalHotKey(SDLKey key) {
	if (key == SDLK_F10) {
		isVisible = !isVisible;
	} else {
		assert(false);
	}
}
 
// Takes keys from the keyboard and inputs them to the console
void SDLConsole::signalEvent(SDL_Event &event)
{
	if (!isVisible) return;
	
	assert (event.type == SDL_KEYDOWN);
	switch (event.key.keysym.sym) {
	case SDLK_PAGEUP:
		if (consoleScrollBack < totalConsoleLines &&
		    consoleScrollBack < NUM_LINES &&
		    NUM_LINES - consoleSurface->h / font->height() > consoleScrollBack + 1) {
			consoleScrollBack++;
			updateConsole();
		}
		break;
	case SDLK_PAGEDOWN:
		if (consoleScrollBack > 0) {
			consoleScrollBack--;
			updateConsole();
		}
		break;
	case SDLK_END:
		consoleScrollBack = 0;
		updateConsole();
		break;
	case SDLK_UP:
		if (commandScrollBack < totalCommands) {
			// move back a line in the command strings and copy
			// the command to the current input string
			commandScrollBack++;
			memset(consoleLines[0], 0, CHARS_PER_LINE);
			strcpy(consoleLines[0], commandLines[commandScrollBack]);
			cursorLocation = strlen(commandLines[commandScrollBack]);
			updateConsole();
		}
		break;
	case SDLK_DOWN:
		if (commandScrollBack > 0) {
			// move forward a line in the command strings and copy
			// the command to the current input string
			commandScrollBack--;
			memset(consoleLines[0], 0, CHARS_PER_LINE);
			strcpy(consoleLines[0], commandLines[commandScrollBack]);
			cursorLocation = strlen(consoleLines[commandScrollBack]);
			updateConsole();
		}
		break;
	case SDLK_BACKSPACE:
		if(cursorLocation > 0) {
			consoleLines[0][cursorLocation-1] = '\0';
			cursorLocation--;
			SDL_Rect inputBackground2;
			inputBackground2.x = 0;
			inputBackground2.y = consoleSurface->h - font->height();
			inputBackground2.w = consoleSurface->w;
			inputBackground2.h = font->height();
			SDL_BlitSurface(inputBackground, NULL, consoleSurface, &inputBackground2);
		}
		break;
	case SDLK_TAB:
		tabCompletion();
		break;
	case SDLK_RETURN:
		newLineCommand();
		// copy the input into the past commands strings
		strcpy(commandLines[0], consoleLines[0]);
		strcpy(consoleLines[1], consoleLines[0]);
		commandExecute(std::string(consoleLines[0]));

		// zero out the current string and get it ready for new input
		memset(consoleLines[0], 0, CHARS_PER_LINE);
		commandScrollBack = -1;
		cursorLocation = 0;
		updateConsole();
		break;
	default:
		if (cursorLocation < CHARS_PER_LINE - 1 && event.key.keysym.unicode) {
			consoleLines[0][cursorLocation] = (char)event.key.keysym.unicode;
			cursorLocation++;
			SDL_Rect inputBackground2;
			inputBackground2.x = 0;
			inputBackground2.y = consoleSurface->h - font->height();
			inputBackground2.w = consoleSurface->w;
			inputBackground2.h = font->height();
			SDL_BlitSurface(inputBackground, NULL, consoleSurface, &inputBackground2);
		}
	}
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

	/* Draw the text from the back buffers, calculate in the scrollback from the user
	 * this is a normal SDL software-mode blit, so we need to temporarily set the ColorKey
	 * for the font, and then clear it when we're done.
	 */
	if ((outputScreen->flags & SDL_OPENGLBLIT) && (outputScreen->format->BytesPerPixel > 2)) {
		Uint32 *pix = (Uint32 *) (font->fontSurface->pixels);
		SDL_SetColorKey(font->fontSurface, SDL_SRCCOLORKEY, *pix);
	}
	int screenlines = consoleSurface->h / font->height();
	for (int loop=0; loop<screenlines-1 && loop<NUM_LINES-1; loop++)
		font->drawText(consoleLines[screenlines-loop+consoleScrollBack-1],
		               consoleSurface, CHAR_BORDER, loop*font->height());
	if (outputScreen->flags & SDL_OPENGLBLIT)
		SDL_SetColorKey(font->fontSurface, 0, 0);
}

// Draws the console buffer to the screen
void SDLConsole::drawConsole()
{
	if (!isVisible) return;
	
	// Update the command line since it has a blinking cursor
	drawCommandLine();

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


// Initializes the console
void SDLConsole::init(const char *fontName, SDL_Surface *displayScreen, SDL_Rect rect)
{
	backgroundImage = NULL;
	consoleAlpha = SDL_ALPHA_OPAQUE;
	outputScreen = displayScreen;

	// Load the consoles font
	font = new SDLFont(fontName, SDLFont::TRANS);	// TODO check for error

	/* make sure that the size of the console is valid */
	if (rect.w > outputScreen->w || rect.w < font->width() * 32)
		rect.w = outputScreen->w;
	if (rect.h > outputScreen->h || rect.h < font->height())
		rect.h = outputScreen->h;
	if (rect.x < 0 || rect.x > outputScreen->w - rect.w)
		dispX = 0;
	else
		dispX = rect.x;
	if (rect.y < 0 || rect.y > outputScreen->h - rect.h)
		dispY = 0;
	else
		dispY = rect.y;

	// load the console surface
	SDL_Surface *temp = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h,
	                                        outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if (temp == NULL) {
		// TODO throw exception
	}
	consoleSurface = SDL_DisplayFormat(temp);
	SDL_FreeSurface(temp);
	SDL_FillRect(consoleSurface, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));

	// Load the dirty rectangle for user input
	temp = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, font->height(),
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if (temp == NULL) {
		// Couldn't create the input background
		// TODO throw exception
	}
	inputBackground = SDL_DisplayFormat(temp);
	SDL_FreeSurface(temp);
	SDL_FillRect(inputBackground, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));

	print("Console initialised.");
}

// Draws the command line the user is typing in to the screen
void SDLConsole::drawCommandLine()
{
	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime) {
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		if (blink) {
			blink = false;
			// The line was being drawn before, now it must be blacked out
			SDL_Rect rect;
			rect.x = strlen(consoleLines[0]) * font->width() + CHAR_BORDER;
			rect.y = consoleSurface->h - font->height();
			rect.w = font->width();
			rect.h = font->height();
			SDL_FillRect(consoleSurface, &rect, 
			             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));
			// Now draw the background image if applicable
			if (backgroundImage) {
				SDL_Rect rect2;
				rect2.x = strlen(consoleLines[0])*font->width() + CHAR_BORDER;
				rect.x = rect2.x - backX;
				rect2.y = consoleSurface->h - font->height();
				rect.y = rect2.y - backY;
				rect2.w = rect.w = font->width();
				rect2.h = rect.h = font->height();
				SDL_BlitSurface(backgroundImage, &rect, consoleSurface, &rect2);
			}
		} else {
			blink = true;
		}
	}

	// If there is enough buffer space add a cursor if it's time to Blink '_' */
	// once again we're drawing text, so in OpenGL context we need to temporarily set up
	// software-mode transparency.
	if (outputScreen->flags & SDL_OPENGLBLIT) {
		Uint32 *pix = (Uint32 *) (font->fontSurface->pixels);
		SDL_SetColorKey(font->fontSurface, SDL_SRCCOLORKEY, *pix);
	}
	if (blink && strlen(consoleLines[0])+1 < (unsigned)CHARS_PER_LINE) {
		char temp[CHARS_PER_LINE];
		strcpy(temp, consoleLines[0]);
		temp[strlen(consoleLines[0])] = '_';
		temp[strlen(consoleLines[0]) + 1] = '\0';
		font->drawText(temp, consoleSurface, CHAR_BORDER, consoleSurface->h - font->height());
	} else {
		// Not time to blink or the strings too long, just draw it
		font->drawText(consoleLines[0], consoleSurface, CHAR_BORDER, consoleSurface->h - font->height());
	}
	if (outputScreen->flags & SDL_OPENGLBLIT) {
		SDL_SetColorKey(font->fontSurface, 0, 0);
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


// Adds  background image to the console, x and y based on consoles x and y
void SDLConsole::background(const char *image, int x, int y)
{
	SDL_Surface *temp;
	SDL_Rect backgroundsrc, backgrounddest;

	// Free the background from the console
	if (image == NULL && backgroundImage != NULL) {
		SDL_FreeSurface(backgroundImage);
		backgroundImage = NULL;
		SDL_FillRect(inputBackground, NULL, 
		             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, SDL_ALPHA_OPAQUE));
		return; 
	}

	// Load a new background
	if (NULL == (temp = SDL_LoadBMP(image))) {
		print("Cannot load background.");
		return;
	}

	// Remove the existing background if it's there
	if(backgroundImage != NULL)
		SDL_FreeSurface(backgroundImage);

	backgroundImage = SDL_DisplayFormat(temp);
	SDL_FreeSurface(temp);
	backX = x;
	backY = y;

	backgroundsrc.x = 0;
	backgroundsrc.y = consoleSurface->h - font->height() - backY;
	backgroundsrc.w = backgroundImage->w;
	backgroundsrc.h = inputBackground->h;

	backgrounddest.x = backX;
	backgrounddest.y = 0;
	backgrounddest.w = backgroundImage->w;
	backgrounddest.h = font->height();

	SDL_FillRect(inputBackground, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, SDL_ALPHA_OPAQUE));
	SDL_BlitSurface(backgroundImage, &backgroundsrc, inputBackground, &backgrounddest);
}

// takes a new x and y of the top left of the console window
void SDLConsole::position(int x, int y)
{
	if (x<0 || x > outputScreen->w - consoleSurface->w)
		dispX = 0;
	else
		dispX = x;

	if (y<0 || y > outputScreen->h - consoleSurface->h)
		dispY = 0;
	else
		dispY = y;
}

// resizes the console, has to reset alot of stuff
void SDLConsole::resize(SDL_Rect rect)
{
	// make sure that the size of the console is valid
	if (rect.w > outputScreen->w || rect.w < font->width() * 32)
		rect.w = outputScreen->w;
	if (rect.h > outputScreen->h || rect.h < font->height())
		rect.h = outputScreen->h;
	if (rect.x < 0 || rect.x > outputScreen->w - rect.w)
		dispX = 0;
	else
		dispX = rect.x;
	if(rect.y < 0 || rect.y > outputScreen->h - rect.h)
		dispY = 0;
	else
		dispY = rect.y;

	// load the console surface
	SDL_FreeSurface(consoleSurface);
	SDL_Surface *temp = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, 
	                                         outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if (temp == NULL) {
		// Couldn't create the consoleSurface
		// TODO throw exception
		return;
	}
	consoleSurface = SDL_DisplayFormat(temp);
	SDL_FreeSurface(temp);

	/* Load the dirty rectangle for user input */
	SDL_FreeSurface(inputBackground);
	temp = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, font->height(), outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if(temp == NULL) {
		// Couldn't create the input background
		// TODO throw exception
		return;
	}
	inputBackground = SDL_DisplayFormat(temp);
	SDL_FreeSurface(temp);

	// Now reset some stuff dependent on the previous size
	consoleScrollBack = 0;

	// Reload the background image (for the input text area) in the console
	if (backgroundImage) {
		SDL_Rect backgroundsrc;
		backgroundsrc.x = 0;
		backgroundsrc.y = consoleSurface->h - font->height() - backY;
		backgroundsrc.w = backgroundImage->w;
		backgroundsrc.h = inputBackground->h;

		SDL_Rect backgrounddest;
		backgrounddest.x = backX;
		backgrounddest.y = 0;
		backgrounddest.w = backgroundImage->w;
		backgrounddest.h = font->height();

		SDL_FillRect(inputBackground, NULL, 
		             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, SDL_ALPHA_OPAQUE));
		SDL_BlitSurface(backgroundImage, &backgroundsrc, inputBackground, &backgrounddest);
	}
}

/*
 * Return the pixel value at (x, y)
 * NOTE: The surface must be locked before calling this!
 */
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

/*
 * Set the pixel at (x, y) to the given value
 * NOTE: The surface must be locked before calling this!
 */
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
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
			{
				p[0] = (pixel >> 16) & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = pixel & 0xff;
			}
			else
			{
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
