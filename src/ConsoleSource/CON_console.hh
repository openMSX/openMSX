#ifndef __CON_CONSOLE_HH__
#define __CON_CONSOLE_HH__


#define CON_CHARS_PER_LINE   128
#define CON_BLINK_RATE       500
#define CON_CHAR_BORDER      4


/* This is a struct for each consoles data */
typedef struct console_information_td
{
	char **ConsoleLines;		/* List of all the past lines */
	char **CommandLines;		/* List of all the past commands */
	int TotalConsoleLines;		/* Total number of lines in the console */
	int ConsoleScrollBack;		/* How much the users scrolled back in the console */
	int TotalCommands;		/* Number of commands in the Back Commands */
	int FontNumber;			/* This is the number of the font for the console */
	int Line_Buffer;		/* The number of lines in the console */
	int BackX, BackY;		/* Background images x and y coords */
	SDL_Surface *ConsoleSurface;	/* Surface of the console */
	SDL_Surface *OutputScreen;	/* This is the screen to draw the console to */
	SDL_Surface *BackgroundImage;	/* Background image for the console */
	SDL_Surface *InputBackground;	/* Dirty rectangle to draw over behind the users background */
	int DispX, DispY;		/* The top left x and y coords of the console on the display screen */
	unsigned char ConsoleAlpha;	/* The consoles alpha level */
	int StringLocation;		/* Current character location in the current string */
	int CommandScrollBack;		/* How much the users scrolled back in the command lines */
} ConsoleInformation;



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
