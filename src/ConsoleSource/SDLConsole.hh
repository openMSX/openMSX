// $Id$

#ifndef __SDLCONSOLE_HH__
#define __SDLCONSOLE_HH__

#include <SDL/SDL.h>
#include "../EventDistributor.hh"
#include "../HotKey.hh"
#include "SDLFont.hh"
#include "Console.hh"


class SDLConsole : public Console, private EventListener, private HotKeyListener
{
	public:
		virtual ~SDLConsole();
		static SDLConsole* instance();

		virtual void drawConsole();
		
		/**
		 * Every SDLRenderer must registers its SDL_screen.
		 * This is used to influence the result of the Renderer.
		 * The render that instantiates the SDL_Screen needs to
		 * call this method.
		 */
		void hookUpSDLConsole(SDL_Surface *Screen);

	private:
		SDLConsole();
	
		void signalHotKey(SDLKey key);
		void signalEvent(SDL_Event &event);

		void init(const char *FontName, SDL_Surface *DisplayScreen, int lines, SDL_Rect rect);
		void alpha(unsigned char alpha);
		void background(const char *image, int x, int y);
		void position(int x, int y);
		void resize(SDL_Rect rect);
		void updateConsole();
		void setAlphaGL(SDL_Surface *s, int alpha);
		void drawCommandLine();
		void putPixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
		Uint32 getPixel(SDL_Surface *surface, int x, int y);

		static const int BLINK_RATE = 500;
		static const int CHAR_BORDER = 4;

		/** Is the console currently visible
		  */ 
		bool isVisible;

		/** This is the font for the console.
		  */
		SDLFont *font;

		/** Background images x coordinate.
		  */
		int backX;

		/** Background images y coordinate.
		  */
		int backY;

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

		/** The top-left x coordinate of the console on the display screen.
		  */
		int dispX;

		/** The top-left y coordinate of the console on the display screen.
		  */
		int dispY;

		/** The consoles alpha level.
		  */
		unsigned char consoleAlpha;

		/** Is the cursor currently blinking
		  */
		bool blink;

		/** Last time the consoles cursor blinked 
		  */
		Uint32 lastBlinkTime;
};

#endif
