// $Id$

#ifndef __SDLCONSOLE_HH__
#define __SDLCONSOLE_HH__

#include "OSDConsoleRenderer.hh"

struct SDL_Surface;


namespace openmsx {

class Console;

class SDLConsole : public OSDConsoleRenderer
{
	public:
		SDLConsole(Console * console_, SDL_Surface *screen);
		virtual ~SDLConsole();

		virtual bool loadFont(const string &filename);
		virtual bool loadBackground(const string &filename);
		virtual void drawConsole();
		virtual void updateConsole();
		
	private:
		/** This is the font for the console.
		  */
		
		/** Surface of the console.
		  */
		SDL_Surface *consoleSurface;

		/** This is the screen to draw the console to.
		  */
		SDL_Surface *outputScreen;

		/** Background image for the console.
		  */
		SDL_Surface *backgroundImage;

		/** Dirty rectangle to draw over behind the users background.
		  */
		SDL_Surface *inputBackground;

		/** Font Surface for the console to get unblended text
		  */
		SDL_Surface *fontLayer;
	
		/** The top-left x coordinate of the console on the display screen.
		  */
		int dispX;

		/** The top-left y coordinate of the console on the display screen.
		  */
		int dispY;

		/** The consoles alpha level.
		  */
		unsigned char consoleAlpha;

		BackgroundSetting* backgroundSetting;
		FontSetting *fontSetting;
		Console* console;
		
		void alpha(unsigned char alpha);
		void loadBackground();
		void position(int x, int y);
		void resize(SDL_Rect rect);
		void reloadBackground();
		void drawCursor();
		void updateConsole2();
		void updateConsoleRect();
		int zoomSurface(SDL_Surface * src, SDL_Surface * dst, bool smooth);
};

} // namespace openmsx

#endif
