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

namespace openmsx {

SDLConsole::SDLConsole(Console& console_, SDL_Surface* screen)
	: OSDConsoleRenderer(console_),
	  console(console_)
{
	string temp = console.getId();
	blink = false;
	lastBlinkTime = 0;

	outputScreen = screen;

	fontSetting = new FontSetting(this, temp + "font", console.getFont());

	initConsoleSize();
	OSDConsoleRenderer::updateConsoleRect(destRect);

	SDL_PixelFormat* format = outputScreen->format;
	backgroundImage = SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA,
		destRect.w, destRect.h, format->BitsPerPixel,
		format->Rmask, format->Gmask, format->Bmask, format->Amask);
	SDL_SetAlpha(backgroundImage, SDL_SRCALPHA, CONSOLE_ALPHA);

	backgroundSetting = new BackgroundSetting(this, temp + "background",
	                                          console.getBackground());
}

SDLConsole::~SDLConsole()
{
	if (backgroundImage) {
		SDL_FreeSurface(backgroundImage);
	}

	delete fontSetting;
	delete backgroundSetting;
}


// Updates the console buffer
void SDLConsole::updateConsole()
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
		loadBackground(console.getBackground());
	}
}

// Draws the console buffer to the screen
void SDLConsole::drawConsole()
{
	if (!console.isVisible()) {
		return;
	}
	updateConsoleRect();

	// draw the background image if there is one
	if (backgroundImage) {
		SDL_BlitSurface(backgroundImage, NULL, outputScreen, &destRect);
	}

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

// Adds background image to the console
bool SDLConsole::loadBackground(const string& filename)
{
	if (filename.empty()) {
		return false;
	}

	SDL_Surface* pictureSurface;
	try {
		File file(filename);
		pictureSurface = IMG_Load(file.getLocalName().c_str());
	} catch (FileException& e) {
		return false;
	}
	// If file does exist, but cannot be read as an image,
	// IMG_Load returns NULL.
	if (pictureSurface == NULL) {
		return false;
	}

	if (backgroundImage) {
		SDL_FreeSurface(backgroundImage);
	}

	// create a 32 bpp surface that will hold the scaled version
	Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0xff000000;
#endif
	SDL_Surface* scaled32Surface = SDL_CreateRGBSurface(
		SDL_SWSURFACE, destRect.w, destRect.h, 32, rmask, gmask, bmask, amask);
	SDL_Surface* picture32Surface =
		SDL_ConvertSurface(pictureSurface, scaled32Surface->format, SDL_SWSURFACE);
	SDL_FreeSurface(pictureSurface);

	// zoom surface
	zoomSurface (picture32Surface, scaled32Surface, true);
	SDL_FreeSurface(picture32Surface);

	// scan image, are all alpha values the same?
	unsigned width = scaled32Surface->w;
	unsigned height = scaled32Surface->h;
	unsigned pitch = scaled32Surface->pitch / 4;
	Uint32* pixels = (Uint32*)scaled32Surface->pixels;
	Uint32 alpha = pixels[0] & amask;
	bool constant = true;
	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {
			if ((pixels[y * pitch + x] & amask) != alpha) {
				constant = false;
				break;
			}
		}
		if (!constant) break;
	}

	// convert the background to the right format
	if (constant) {
		backgroundImage = SDL_DisplayFormat(scaled32Surface);
		SDL_SetAlpha(backgroundImage, SDL_SRCALPHA, CONSOLE_ALPHA);
	} else {
		backgroundImage = SDL_DisplayFormatAlpha(scaled32Surface);
	}
	SDL_FreeSurface(scaled32Surface);

	return true;
}

bool SDLConsole::loadFont(const string& filename)
{
	if (filename.empty()) {
		return false;
	}
	try {
		File file(filename);
		SDLFont* newFont = new SDLFont(&file);
		newFont->setSurface(outputScreen);
		delete font;
		font = newFont;
	} catch (MSXException &e) {
		return false;
	}
	return true;
}


// ----------------------------------------------------------------------------

// TODO: Replacing this by a 4-entry array would simplify the code.
struct ColorRGBA {
	Uint8 r;
	Uint8 g;
	Uint8 b;
	Uint8 a;
};

int SDLConsole::zoomSurface(SDL_Surface* src, SDL_Surface* dst, bool smooth)
{
	int x, y, sx, sy, *sax, *say, *csax, *csay, csx, csy, ex, ey, t1, t2, sstep;
	ColorRGBA *c00, *c01, *c10, *c11;
	ColorRGBA *sp, *csp, *dp;
	int dgap;

	// Variable setup
	if (smooth) {
		// For interpolation: assume source dimension is one pixel
		// smaller to avoid overflow on right and bottom edge.
		sx = (int) (65536.0 * (float) (src->w - 1) / (float) dst->w);
		sy = (int) (65536.0 * (float) (src->h - 1) / (float) dst->h);
	} else {
		sx = (int) (65536.0 * (float) src->w / (float) dst->w);
		sy = (int) (65536.0 * (float) src->h / (float) dst->h);
	}

	// Allocate memory for row increments
	if ((sax = (int *) malloc((dst->w + 1) * sizeof(Uint32))) == NULL) {
		return -1;
	}
	if ((say = (int *) malloc((dst->h + 1) * sizeof(Uint32))) == NULL) {
		free(sax);
		return -1;
	}

	// Precalculate row increments
	csx = 0;
	csax = sax;
	for (x = 0; x <= dst->w; x++) {
		*csax = csx;
		csax++;
		csx &= 0xffff;
		csx += sx;
	}
	csy = 0;
	csay = say;
	for (y = 0; y <= dst->h; y++) {
		*csay = csy;
		csay++;
		csy &= 0xffff;
		csy += sy;
	}

	// Pointer setup
	sp = csp = (ColorRGBA *) src->pixels;
	dp = (ColorRGBA *) dst->pixels;
	dgap = dst->pitch - dst->w * 4;

	// Switch between interpolating and non-interpolating code
	if (smooth) {
		// Interpolating Zoom
		// Scan destination
		csay = say;
		for (y = 0; y < dst->h; y++) {
			// Setup color source pointers
			c00 = csp;
			c01 = csp;
			c01++;
			c10 = (ColorRGBA *) ((Uint8 *) csp + src->pitch);
			c11 = c10;
			c11++;
			csax = sax;
			for (x = 0; x < dst->w; x++) {
				// Interpolate colors
				ex = (*csax & 0xffff);
				ey = (*csay & 0xffff);
				t1 = ((((c01->r - c00->r) * ex) >> 16) + c00->r) & 0xff;
				t2 = ((((c11->r - c10->r) * ex) >> 16) + c10->r) & 0xff;
				dp->r = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->g - c00->g) * ex) >> 16) + c00->g) & 0xff;
				t2 = ((((c11->g - c10->g) * ex) >> 16) + c10->g) & 0xff;
				dp->g = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->b - c00->b) * ex) >> 16) + c00->b) & 0xff;
				t2 = ((((c11->b - c10->b) * ex) >> 16) + c10->b) & 0xff;
				dp->b = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->a - c00->a) * ex) >> 16) + c00->a) & 0xff;
				t2 = ((((c11->a - c10->a) * ex) >> 16) + c10->a) & 0xff;
				dp->a = (((t2 - t1) * ey) >> 16) + t1;

				// Advance source pointers
				csax++;
				sstep = (*csax >> 16);
				c00 += sstep;
				c01 += sstep;
				c10 += sstep;
				c11 += sstep;
				// Advance destination pointer
				dp++;
			}
			// Advance source pointer
			csay++;
			csp = (ColorRGBA *) ((Uint8 *) csp + (*csay >> 16) * src->pitch);
			// Advance destination pointers
			dp = (ColorRGBA *) ((Uint8 *) dp + dgap);
		}
	} else {
		// Non-Interpolating Zoom
		csay = say;
		for (y = 0; y < dst->h; y++) {
			sp = csp;
			csax = sax;
			for (x = 0; x < dst->w; x++) {
				// Draw
				*dp = *sp;
				// Advance source pointers
				csax++;
				sp += (*csax >> 16);
				// Advance destination pointer
				dp++;
			}
			// Advance source pointer
			csay++;
			csp = (ColorRGBA *) ((Uint8 *) csp + (*csay >> 16) * src->pitch);
			// Advance destination pointers
			dp = (ColorRGBA *) ((Uint8 *) dp + dgap);
		}
	}

	// Remove temp arrays
	free(sax);
	free(say);

	return 0;
}

} // namespace openmsx
