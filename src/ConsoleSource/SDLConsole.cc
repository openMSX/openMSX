// $Id$

/*  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Adapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <cassert>

#include "SDLConsole.hh"
#include "SDLFont.hh"
#include "MSXConfig.hh"
#include "File.hh"
#include "Console.hh"

SDLConsole::SDLConsole(SDL_Surface *screen)
{ 
	console = Console::instance();
	
	blink = false;
	lastBlinkTime = 0;
	
	outputScreen = screen;
	backgroundImage = NULL;
	consoleSurface  = NULL;
	inputBackground = NULL;
	fontLayer = NULL;
	
	fontSetting = new FontSetting(this, fontName);
	initConsoleSize();
	
	SDL_Rect rect;
	OSDConsoleRenderer::updateConsoleRect(rect);
	
	resize(rect);
	backgroundSetting = new BackgroundSetting(this, backgroundName);
	alpha(CONSOLE_ALPHA);
	
}

SDLConsole::~SDLConsole()
{
	if (inputBackground) {
		SDL_FreeSurface(inputBackground);
	}
	if (consoleSurface) {
		SDL_FreeSurface(consoleSurface);
	}
	if (backgroundImage) {
		SDL_FreeSurface(backgroundImage);
	}
	if (fontLayer){
		SDL_FreeSurface(fontLayer);
	}	
		
	delete fontSetting;
	delete backgroundSetting;
}


// Updates the console buffer
void SDLConsole::updateConsole()
{
	if (!console->isVisible()) {
		return;
	}
	updateConsole2();
}

void SDLConsole::updateConsole2()
{
	SDL_FillRect(fontLayer, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, 0));
	// when using SDL_RLEACCEL we get a segfault !!
	SDL_SetColorKey(fontLayer, SDL_SRCCOLORKEY ,0);  
	
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
	for (int loop = 0; loop < screenlines; loop++) {
		int num = loop + console->getScrollBack();
		font->drawText(console->getLine(num), CHAR_BORDER,
		       consoleSurface->h - (1+loop)*font->getHeight());
	}
}

void SDLConsole::updateConsoleRect()
{
	SDL_Rect rect;
	OSDConsoleRenderer::updateConsoleRect(rect);
	if ((consoleSurface->h != rect.h) || (consoleSurface->w != rect.w) 
		|| (dispX != rect.x) || (dispY != rect.y))
	{
		resize(rect);
		alpha(CONSOLE_ALPHA);
		loadBackground(backgroundName);
		updateConsole2();
	}		
}

// Draws the console buffer to the screen
void SDLConsole::drawConsole()
{
	if (!console->isVisible()) {
		return;
	}
	updateConsoleRect();
	drawCursor();

	// Setup the rect the console is being blitted into based on the output screen
	SDL_Rect destRect;
	destRect.x = dispX;
	destRect.y = dispY;
	destRect.w = consoleSurface->w;
	destRect.h = consoleSurface->h;
	SDL_BlitSurface(consoleSurface, NULL, outputScreen, &destRect);
	SDL_BlitSurface(fontLayer,NULL,outputScreen,&destRect);
}


// Draws the command line the user is typing in to the screen
void SDLConsole::drawCursor()
{
	int cursorLocation = console->getCursorPosition();
	// Check if the blink period is over
	if (SDL_GetTicks() > lastBlinkTime){
		lastBlinkTime = SDL_GetTicks() + BLINK_RATE;
		blink = !blink;
		if (console->getScrollBack() != 0) {
			return;
		}
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
			font->drawText(console->getLine(0).substr(cursorLocation,cursorLocation),
				      CHAR_BORDER + cursorLocation * font->getWidth(),
				      consoleSurface->h - font->getHeight());		
		}
	}
	if (cursorLocation != lastCursorPosition){
		blink=true; // force cursor
		lastBlinkTime=SDL_GetTicks() + BLINK_RATE; // maximum time
		lastCursorPosition=cursorLocation;
		font->drawText(std::string("_"),
		      CHAR_BORDER + cursorLocation * font->getWidth(),
		      consoleSurface->h - font->getHeight());
	}
}

// Sets the alpha level of the console, 255 turns off alpha blending
void SDLConsole::alpha(unsigned char newAlpha)
{
	// store alpha as state!
	consoleAlpha = newAlpha;
	if (consoleAlpha == 255) {
		SDL_SetAlpha(consoleSurface, 0,            consoleAlpha);
	} else {
		SDL_SetAlpha(consoleSurface, SDL_SRCALPHA, consoleAlpha);
	}
	updateConsole2();
}

// Adds background image to the console
bool SDLConsole::loadBackground(const std::string &filename)
{
	if (filename.empty()) {
		return false;
	}
	File file(filename);
	SDL_Surface *pictureSurface = IMG_Load(file.getLocalName().c_str());
	if (pictureSurface == NULL) {
		return false;
	}
	if (backgroundImage) {
		SDL_FreeSurface(backgroundImage);
	}
	SDL_Rect rect;
	OSDConsoleRenderer::updateConsoleRect(rect); // get the size

	// create a 32 bpp surface that will hold the scaled version
	SDL_Surface * scaled32Surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
									rect.w, rect.h, 32, 0, 0, 0, 0);
	// convert the picturesurface to 32 bpp
	SDL_PixelFormat * format=scaled32Surface->format;
	SDL_Surface * picture32Surface = SDL_ConvertSurface(pictureSurface,format,0);

	SDL_FreeSurface(pictureSurface);
	zoomSurface (picture32Surface,scaled32Surface,1);
	SDL_FreeSurface(picture32Surface);
	// convert the background to the right format
	backgroundImage = SDL_DisplayFormat(scaled32Surface);
	SDL_FreeSurface(scaled32Surface);

	reloadBackground();
	return true;
}

bool SDLConsole::loadFont(const std::string &filename)
{
	if (filename.empty()) {
		return false;
	}
	try {
		File file(filename);
		SDLFont* newFont = new SDLFont(&file);
		newFont->setSurface(fontLayer);
		delete font;
		font = newFont;
	} catch (MSXException &e) {
		return false;
	}
	return true;
}

// resizes the console, has to reset alot of stuff
void SDLConsole::resize(SDL_Rect rect)
{
	// make sure that the size of the console is valid
	assert (!(rect.w > outputScreen->w || rect.w < font->getWidth() * 32));
	assert (!(rect.h > outputScreen->h || rect.h < font->getHeight()));

	SDL_Surface *temp1 = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, 
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	SDL_Surface *temp2 = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h,
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	SDL_Surface *temp3 = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, font->getHeight(),
	                            outputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if (temp1 == NULL || temp2 == NULL || temp3 == NULL)
		return;
	
	if (consoleSurface)  SDL_FreeSurface(consoleSurface);
	if (inputBackground) SDL_FreeSurface(inputBackground);
	if (fontLayer) SDL_FreeSurface(fontLayer);
	
	consoleSurface = SDL_DisplayFormat(temp1);
	fontLayer=SDL_DisplayFormat(temp2);
	
	SDLFont* sdlFont = dynamic_cast<SDLFont*>(font);
	if (sdlFont) {
		sdlFont->setSurface(fontLayer);
	}
	
	inputBackground = SDL_DisplayFormat(temp3);
	SDL_FreeSurface(temp1);
	SDL_FreeSurface(temp2);
	SDL_FreeSurface(temp3);
	SDL_FillRect(consoleSurface, NULL, 
	             SDL_MapRGBA(consoleSurface->format, 0, 0, 0, consoleAlpha));
	
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

int SDLConsole::zoomSurface(SDL_Surface * src, SDL_Surface * dst, int smooth)
{
    int x, y, sx, sy, *sax, *say, *csax, *csay, csx, csy, ex, ey, t1, t2, sstep;
    tColorRGBA *c00, *c01, *c10, *c11;
    tColorRGBA *sp, *csp, *dp;
    int sgap, dgap;

    /*
     * Variable setup
     */
    if (smooth) {
	/*
	 * For interpolation: assume source dimension is one pixel
	 */
	/*
	 * smaller to avoid overflow on right and bottom edge.
	 */
	sx = (int) (65536.0 * (float) (src->w - 1) / (float) dst->w);
	sy = (int) (65536.0 * (float) (src->h - 1) / (float) dst->h);
    } else {
	sx = (int) (65536.0 * (float) src->w / (float) dst->w);
	sy = (int) (65536.0 * (float) src->h / (float) dst->h);
    }

    /*
     * Allocate memory for row increments
     */
    if ((sax = (int *) malloc((dst->w + 1) * sizeof(Uint32))) == NULL) {
	return (-1);
    }
    if ((say = (int *) malloc((dst->h + 1) * sizeof(Uint32))) == NULL) {
	free(sax);
	return (-1);
    }

    /*
     * Precalculate row increments
     */
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

    /*
     * Pointer setup
     */
    sp = csp = (tColorRGBA *) src->pixels;
    dp = (tColorRGBA *) dst->pixels;
    sgap = src->pitch - src->w * 4;
    dgap = dst->pitch - dst->w * 4;

    /*
     * Switch between interpolating and non-interpolating code
     */
    if (smooth) {

	/*
	 * Interpolating Zoom
	 */

	/*
	 * Scan destination
	 */
	csay = say;
	for (y = 0; y < dst->h; y++) {
	    /*
	     * Setup color source pointers
	     */
	    c00 = csp;
	    c01 = csp;
	    c01++;
	    c10 = (tColorRGBA *) ((Uint8 *) csp + src->pitch);
	    c11 = c10;
	    c11++;
	    csax = sax;
	    for (x = 0; x < dst->w; x++) {

		/*
		 * Interpolate colors
		 */
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

		/*
		 * Advance source pointers
		 */
		csax++;
		sstep = (*csax >> 16);
		c00 += sstep;
		c01 += sstep;
		c10 += sstep;
		c11 += sstep;
		/*
		 * Advance destination pointer
		 */
		dp++;
	    }
	    /*
	     * Advance source pointer
	     */
	    csay++;
	    csp = (tColorRGBA *) ((Uint8 *) csp + (*csay >> 16) * src->pitch);
	    /*
	     * Advance destination pointers
	     */
	    dp = (tColorRGBA *) ((Uint8 *) dp + dgap);
	}

    } else {

	/*
	 * Non-Interpolating Zoom
	 */

	csay = say;
	for (y = 0; y < dst->h; y++) {
	    sp = csp;
	    csax = sax;
	    for (x = 0; x < dst->w; x++) {
		/*
		 * Draw
		 */
		*dp = *sp;
		/*
		 * Advance source pointers
		 */
		csax++;
		sp += (*csax >> 16);
		/*
		 * Advance destination pointer
		 */
		dp++;
	    }
	    /*
	     * Advance source pointer
	     */
	    csay++;
	    csp = (tColorRGBA *) ((Uint8 *) csp + (*csay >> 16) * src->pitch);
	    /*
	     * Advance destination pointers
	     */
	    dp = (tColorRGBA *) ((Uint8 *) dp + dgap);
	}

    }

    /*
     * Remove temp arrays
     */
    free(sax);
    free(say);

    return (0);
}
