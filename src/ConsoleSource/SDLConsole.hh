// $Id$

#ifndef __SDLCONSOLE_HH__
#define __SDLCONSOLE_HH__

#include <SDL/SDL.h>
#include "EventListener.hh"
#include "InteractiveConsole.hh"
#include "Command.hh"

// forward declaration
class SDLFont;


class SDLConsole : public InteractiveConsole, private EventListener
{
	public:
		SDLConsole(SDL_Surface *screen);
		virtual ~SDLConsole();

		virtual void drawConsole();

	private:
		void signalEvent(SDL_Event &event);

		void alpha(unsigned char alpha);
		void background(const std::string &image, int x, int y);
		void position(int x, int y);
		void resize(SDL_Rect rect);
		void reloadBackground();
		void updateConsole();
		void setAlphaGL(SDL_Surface *s, int alpha);
		void drawCursor();
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


		class ConsoleCmd : public Command {
			public:
				ConsoleCmd(SDLConsole *cons);
				virtual void execute(const std::vector<std::string> &tokens);
				virtual void help   (const std::vector<std::string> &tokens);
			private:
				SDLConsole *console;
		};
		friend class ConsoleCmd;
		ConsoleCmd consoleCmd;
};

#endif
