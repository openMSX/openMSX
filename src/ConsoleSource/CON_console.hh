// $Id$

#ifndef __CON_CONSOLE_HH__
#define __CON_CONSOLE_HH__


#define CON_CHARS_PER_LINE   128
#define CON_BLINK_RATE       500
#define CON_CHAR_BORDER      4


/** This is a struct for each consoles data.
  * TODO: Transform this into a class.
  */
typedef struct console_information_td
{
	/** List of all the past lines.
	  */
	char **ConsoleLines;

	/** List of all the past commands.
	  * TODO: Call this "history" or "commandHistory".
	  */
	char **CommandLines;

	/** Total number of lines in the console.
	  */
	int TotalConsoleLines;

	/** How much the users scrolled back in the console.
	  */
	int ConsoleScrollBack;

	/** Number of commands in the Back Commands.
	  */
	int TotalCommands;

	/** This is the number of the font for the console.
	  */
	int FontNumber;

	/** The number of lines in the console.
	  */
	int Line_Buffer;

	/** Background images x coordinate.
	  */
	int BackX;

	/** Background images y coordinate.
	  */
	int BackY;

	/** Surface of the console.
	  */
	SDL_Surface *ConsoleSurface;

	/** This is the screen to draw the console to.
	  */
	SDL_Surface *OutputScreen;

	/** Background image for the console.
	  */
	SDL_Surface *BackgroundImage;

	/** Dirty rectangle to draw over behind the users background.
	  */
	SDL_Surface *InputBackground;

	/** The top-left x coordinate of the console on the display screen.
	  */
	int DispX;

	/** The top-left y coordinate of the console on the display screen.
	  */
	int DispY;

	/** The consoles alpha level.
	  */
	unsigned char ConsoleAlpha;

	/** Current character location in the current string.
	  */
	int StringLocation;

	/** How much the users scrolled back in the command lines.
	  */
	int CommandScrollBack;

} ConsoleInformation;


// TODO: Documentation of these functions is currently in CON_console.cc;
//       it should be moved here.
void	CON_Events(SDL_Event *event);
void	CON_DrawConsole(ConsoleInformation *console);
ConsoleInformation	*CON_Init(const char *FontName, SDL_Surface *DisplayScreen, int lines, SDL_Rect rect);
void	CON_Destroy(ConsoleInformation *console);
void	CON_Out(ConsoleInformation *console, const char *str, ...);
void	CON_Alpha(ConsoleInformation *console, unsigned char alpha);
int	CON_Background(ConsoleInformation *console, const char *image, int x, int y);
void	CON_Position(ConsoleInformation *console, int x, int y);
int	CON_Resize(ConsoleInformation *console, SDL_Rect rect);
void	CON_NewLineConsole(ConsoleInformation *console);
void	CON_NewLineCommand(ConsoleInformation *console);
void	CON_UpdateConsole(ConsoleInformation *console);
void	CON_Topmost(ConsoleInformation *console);

#endif
