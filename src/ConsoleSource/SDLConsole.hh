// $Id$

#ifndef __SDLCONSOLE_HH__
#define __SDLCONSOLE_HH__

#include "SDLInteractiveConsole.hh"

// forward declaration
class SDLFont;
class SDL_Surface;
class Config;


class SDLConsole : public SDLInteractiveConsole
{
	public:
		SDLConsole(SDL_Surface *screen);
		virtual ~SDLConsole();

		virtual void drawConsole();
		virtual bool loadBackground(const std::string &filename);

	private:
		virtual void updateConsole();

		void alpha(unsigned char alpha);
		void loadBackground();
		void position(int x, int y);
		void resize(SDL_Rect rect);
		void reloadBackground();
		void drawCursor();

		static const int BLINK_RATE = 500;
		static const int CHAR_BORDER = 4;

		// This is the font for the console.
		SDLFont *font;

		// Surface of the console.
		SDL_Surface *consoleSurface;

		// This is the screen to draw the console to.
		SDL_Surface *outputScreen;

		// Background image for the console.
		SDL_Surface *backgroundImage;

		// Dirty rectangle to draw over behind the users background.
		SDL_Surface *inputBackground;

		// The top-left x coordinate of the console on the display screen.
		int dispX;

		// The top-left y coordinate of the console on the display screen.
		int dispY;

		// The consoles alpha level.
		unsigned char consoleAlpha;

		// Is the cursor currently blinking
		bool blink;

		// Last time the consoles cursor blinked 
		Uint32 lastBlinkTime;
	
		// 
		BackgroundSetting* backgroundSetting;
};

#endif
