// $Id$

/*  CON_console.c
 *  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Addapted to C++ and openMSX needs by David Heremans
 *  This is free, just be sure to give me credit when using it
 *  in any of your programs.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "SDL.h"
#include "CON_console.hh"
#include "CON_consolecommands.hh"
#include "DT_drawtext.hh"
#include "CON_internal.hh"


/* This contains a pointer to the "topmost" console. The console that
 * is currently taking keyboard input. */
static ConsoleInformation	*Topmost;

/* Forward declaration, this function is only used internally. */
static void DrawCommandLine();

/* Takes keys from the keyboard and inputs them to the console */
void CON_Events(SDL_Event *event)
{
	SDL_Rect inputbackground;


	if(Topmost != NULL && event->type == SDL_KEYDOWN)
	{
		switch (event->key.keysym.sym)
		{
			case SDLK_PAGEUP:
				if(Topmost->ConsoleScrollBack < Topmost->TotalConsoleLines && Topmost->ConsoleScrollBack < Topmost->Line_Buffer && Topmost->Line_Buffer - Topmost->ConsoleSurface->h / DT_FontHeight(Topmost->FontNumber) > Topmost->ConsoleScrollBack + 1)
				{
					Topmost->ConsoleScrollBack++;
					CON_UpdateConsole(Topmost);
				}
				break;
			case SDLK_PAGEDOWN:
				if(Topmost->ConsoleScrollBack > 0)
				{
					Topmost->ConsoleScrollBack--;
					CON_UpdateConsole(Topmost);
				}
				break;
			case SDLK_END:
				Topmost->ConsoleScrollBack = 0;
				CON_UpdateConsole(Topmost);
				break;
			case SDLK_UP:
				if(Topmost->CommandScrollBack < Topmost->TotalCommands)
				{
					/* move back a line in the command strings and copy the command to the current input string */
					Topmost->CommandScrollBack++;
					memset(Topmost->ConsoleLines[0], 0, CON_CHARS_PER_LINE);
					strcpy(Topmost->ConsoleLines[0], Topmost->CommandLines[Topmost->CommandScrollBack]);
					Topmost->StringLocation = strlen(Topmost->CommandLines[Topmost->CommandScrollBack]);
					CON_UpdateConsole(Topmost);
				}
				break;
			case SDLK_DOWN:
				if(Topmost->CommandScrollBack > 0)
				{
					/* move forward a line in the command strings and copy the command to the current input string */
					Topmost->CommandScrollBack--;
					memset(Topmost->ConsoleLines[0], 0, CON_CHARS_PER_LINE);
					strcpy(Topmost->ConsoleLines[0], Topmost->CommandLines[Topmost->CommandScrollBack]);
					Topmost->StringLocation = strlen(Topmost->ConsoleLines[Topmost->CommandScrollBack]);
					CON_UpdateConsole(Topmost);
				}
				break;
			case SDLK_BACKSPACE:
				if(Topmost->StringLocation > 0)
				{
					Topmost->ConsoleLines[0][Topmost->StringLocation - 1] = '\0';
					Topmost->StringLocation--;
					inputbackground.x = 0;
					inputbackground.y = Topmost->ConsoleSurface->h - DT_FontHeight(Topmost->FontNumber);
					inputbackground.w = Topmost->ConsoleSurface->w;
					inputbackground.h = DT_FontHeight(Topmost->FontNumber);
					SDL_BlitSurface(Topmost->InputBackground, NULL, Topmost->ConsoleSurface, &inputbackground);

				}
				break;
			case SDLK_TAB:
				CON_TabCompletion(Topmost);
				break;
			case SDLK_RETURN:
				CON_NewLineCommand(Topmost);
				/* copy the input into the past commands strings */
				strcpy(Topmost->CommandLines[0], Topmost->ConsoleLines[0]);
				strcpy(Topmost->ConsoleLines[1], Topmost->ConsoleLines[0]);
				CON_CommandExecute(Topmost);

				/* zero out the current string and get it ready for new input */
				memset(Topmost->ConsoleLines[0], 0, CON_CHARS_PER_LINE);
				Topmost->CommandScrollBack = -1;
				Topmost->StringLocation = 0;
				CON_UpdateConsole(Topmost);
				break;
			default:
				if(Topmost->StringLocation < CON_CHARS_PER_LINE - 1 && event->key.keysym.unicode)
				{
					Topmost->ConsoleLines[0][Topmost->StringLocation] = (char)event->key.keysym.unicode;
					Topmost->StringLocation++;
					inputbackground.x = 0;
					inputbackground.y = Topmost->ConsoleSurface->h - DT_FontHeight(Topmost->FontNumber);
					inputbackground.w = Topmost->ConsoleSurface->w;
					inputbackground.h = DT_FontHeight(Topmost->FontNumber);
					SDL_BlitSurface(Topmost->InputBackground, NULL, Topmost->ConsoleSurface, &inputbackground);
				}
		}
	}
}

/* CON_SetAlphaGL() -- sets the alpha channel of an SDL_Surface to the
 * specified value.  Preconditions: the surface in question is RGBA.
 * 0 <= a <= 255, where 0 is transparent and 255 is opaque. */
void CON_SetAlphaGL(SDL_Surface *s, int alpha)
{
	Uint8 val;
	int x, y, w, h;
	Uint32 pixel;
	Uint8 r, g, b, a;
	SDL_PixelFormat *format;
	static char errorPrinted = 0;


	/* debugging assertions -- these slow you down, but hey, crashing sucks */
	if(!s)
	{
		PRINT_ERROR("NULL Surface passed to CON_SetAlphaGL\n");
		return;
	}

	/* clamp alpha value to 0...255 */
	if(alpha < SDL_ALPHA_TRANSPARENT)
		val = SDL_ALPHA_TRANSPARENT;
	else if(alpha > SDL_ALPHA_OPAQUE)
		val = SDL_ALPHA_OPAQUE;
	else
		val = alpha;

	/* loop over alpha channels of each pixel, setting them appropriately. */
	w = s->w;
	h = s->h;
	format = s->format;
	switch (format->BytesPerPixel)
	{
		case 2:
			/* 16-bit surfaces don't seem to support alpha channels. */
			if(!errorPrinted)
			{
				errorPrinted = 1;
				PRINT_ERROR("16-bit SDL surfaces do not support alpha-blending under OpenGL.\n");
			}
			break;
		case 4:
			{
				/* we can do this very quickly in 32-bit mode.  24-bit is more
				 * difficult.  And since 24-bit mode is reall the same as 32-bit,
				 * so it usually ends up taking this route too.  Win!  Unroll loops
				 * and use pointer arithmetic for extra speed. */
				int numpixels = h * (w << 2);
				Uint8 *pix = (Uint8 *) (s->pixels);
				Uint8 *last = pix + numpixels;
				Uint8 *pixel;
				if(numpixels & 0x7 == 0)
					for(pixel = pix + 3; pixel < last; pixel += 32)
						*pixel = *(pixel + 4) = *(pixel + 8) = *(pixel + 12) = 
*(pixel + 16) = *(pixel + 20) = *(pixel + 24) = *(pixel + 28) = val;
				else
					for(pixel = pix + 3; pixel < last; pixel += 4)
						*pixel = val;
				break;
			}
		default:
			/* we have no choice but to do this slowly.  <sigh> */
			for(y = 0; y < h; ++y)
				for(x = 0; x < w; ++x)
				{
					char print = 0;
					/* Lock the surface for direct access to the pixels */
					if(SDL_MUSTLOCK(s) && SDL_LockSurface(s) < 0)
					{
						PRINT_ERROR("Can't lock surface: ");
						fprintf(stderr, "%s\n", SDL_GetError());
						return;
					}
					pixel = DT_GetPixel(s, x, y);
					if(x == 0 && y == 0)
						print = 1;
					SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
					pixel = SDL_MapRGBA(format, r, g, b, val);
					SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
					DT_PutPixel(s, x, y, pixel);

					/* unlock surface again */
					if(SDL_MUSTLOCK(s))
						SDL_UnlockSurface(s);
				}
			break;
	}
}


/* Updates the console buffer */
void CON_UpdateConsole(ConsoleInformation *console)
{
	int loop;
	int Screenlines = console->ConsoleSurface->h / DT_FontHeight(console->FontNumber);
	SDL_Rect DestRect;
	BitFont *CurrentFont = DT_FontPointer(console->FontNumber);



	SDL_FillRect(console->ConsoleSurface, NULL, SDL_MapRGBA(console->ConsoleSurface->format, 0, 0, 0, console->ConsoleAlpha));

	if(console->OutputScreen->flags & SDL_OPENGLBLIT)
		SDL_SetAlpha(console->ConsoleSurface, 0, SDL_ALPHA_OPAQUE);

	/* draw the background image if there is one */
	if(console->BackgroundImage)
	{
		DestRect.x = console->BackX;
		DestRect.y = console->BackY;
		DestRect.w = console->BackgroundImage->w;
		DestRect.h = console->BackgroundImage->h;
		SDL_BlitSurface(console->BackgroundImage, NULL, console->ConsoleSurface, &DestRect);
	}

	/* Draw the text from the back buffers, calculate in the scrollback from the user
	 * this is a normal SDL software-mode blit, so we need to temporarily set the ColorKey
	 * for the font, and then clear it when we're done.
	 */
	if((console->OutputScreen->flags & SDL_OPENGLBLIT) && (console->OutputScreen->format->BytesPerPixel > 2))
	{
		Uint32 *pix = (Uint32 *) (CurrentFont->FontSurface->pixels);
		SDL_SetColorKey(CurrentFont->FontSurface, SDL_SRCCOLORKEY, *pix);
	}
	for(loop = 0; loop < Screenlines - 1 && loop < console->Line_Buffer - 1; loop++)
		DT_DrawText(console->ConsoleLines[Screenlines - loop + console->ConsoleScrollBack - 1], console->ConsoleSurface, console->FontNumber, CON_CHAR_BORDER, loop * DT_FontHeight(console->FontNumber));

	if(console->OutputScreen->flags & SDL_OPENGLBLIT)
		SDL_SetColorKey(CurrentFont->FontSurface, 0, 0);
}

/* Draws the console buffer to the screen */
void CON_DrawConsole(ConsoleInformation *console)
{
	SDL_Rect DestRect;


	/* Update the command line since it has a blinking cursor */
	if(Topmost) DrawCommandLine();

	/* before drawing, make sure the alpha channel of the console surface is set
	 * properly.  (sigh) I wish we didn't have to do this every frame... */
	if(console->OutputScreen->flags & SDL_OPENGLBLIT)
		CON_SetAlphaGL(console->ConsoleSurface, console->ConsoleAlpha);

	/* Setup the rect the console is being blitted into based on the output screen */
	DestRect.x = console->DispX;
	DestRect.y = console->DispY;
	DestRect.w = console->ConsoleSurface->w;
	DestRect.h = console->ConsoleSurface->h;

	SDL_BlitSurface(console->ConsoleSurface, NULL, console->OutputScreen, &DestRect);

	if(console->OutputScreen->flags & SDL_OPENGLBLIT)
		SDL_UpdateRects(console->OutputScreen, 1, &DestRect);
}


/* Initializes the console */
ConsoleInformation *CON_Init(const char *FontName, SDL_Surface *DisplayScreen, int lines, SDL_Rect rect)
{
	int loop;
	SDL_Surface *Temp;
	ConsoleInformation *newinfo;


	/* Create a new console struct and init it. */
	if((newinfo = (ConsoleInformation *) malloc(sizeof(ConsoleInformation))) == NULL)
	{
		PRINT_ERROR("Could not allocate the space for a new console info struct.\n");
		return NULL;
	}
	newinfo->ConsoleLines = NULL;
	newinfo->CommandLines = NULL;
	newinfo->TotalConsoleLines = 0;
	newinfo->ConsoleScrollBack = 0;
	newinfo->TotalCommands = 0;
	newinfo->BackgroundImage = NULL;
	newinfo->ConsoleAlpha = SDL_ALPHA_OPAQUE;
	newinfo->StringLocation = 0;
	newinfo->CommandScrollBack = 0;
	newinfo->OutputScreen = DisplayScreen;

	/* Load the consoles font */
	if(-1 == (newinfo->FontNumber = DT_LoadFont(FontName, TRANS_FONT)))
	{
		PRINT_ERROR("Could not load the font ");
		fprintf(stderr, "\"%s\" for the console!\n", FontName);
		return NULL;
	}

	/* make sure that the size of the console is valid */
	if(rect.w > newinfo->OutputScreen->w || rect.w < DT_FontWidth(newinfo->FontNumber) * 32)
		rect.w = newinfo->OutputScreen->w;
	if(rect.h > newinfo->OutputScreen->h || rect.h < DT_FontHeight(newinfo->FontNumber))
		rect.h = newinfo->OutputScreen->h;
	if(rect.x < 0 || rect.x > newinfo->OutputScreen->w - rect.w)
		newinfo->DispX = 0;
	else
		newinfo->DispX = rect.x;
	if(rect.y < 0 || rect.y > newinfo->OutputScreen->h - rect.h)
		newinfo->DispY = 0;
	else
		newinfo->DispY = rect.y;

	/* load the console surface */
	Temp = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, newinfo->OutputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if(Temp == NULL)
	{
		PRINT_ERROR("Couldn't create the ConsoleSurface\n");
		return NULL;
	}
	newinfo->ConsoleSurface = SDL_DisplayFormat(Temp);
	SDL_FreeSurface(Temp);
	SDL_FillRect(newinfo->ConsoleSurface, NULL, SDL_MapRGBA(newinfo->ConsoleSurface->format, 0, 0, 0, newinfo->ConsoleAlpha));

	/* We would like to have a minumum # of lines to guarentee we don't create a memory error */
	if(rect.h / DT_FontHeight(newinfo->FontNumber) > lines)
		newinfo->Line_Buffer = rect.h / DT_FontHeight(newinfo->FontNumber);
	else
		newinfo->Line_Buffer = lines;


	newinfo->ConsoleLines = (char **)malloc(sizeof(char *) * newinfo->Line_Buffer);
	newinfo->CommandLines = (char **)malloc(sizeof(char *) * newinfo->Line_Buffer);
	for(loop = 0; loop <= newinfo->Line_Buffer - 1; loop++)
	{
		newinfo->ConsoleLines[loop] = (char *)calloc(CON_CHARS_PER_LINE, sizeof(char));
		newinfo->CommandLines[loop] = (char *)calloc(CON_CHARS_PER_LINE, sizeof(char));
	}

	/* Load the dirty rectangle for user input */
	Temp = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, DT_FontHeight(newinfo->FontNumber), newinfo->OutputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if(Temp == NULL)
	{
		PRINT_ERROR("Couldn't create the input background\n");
		return NULL;
	}
	newinfo->InputBackground = SDL_DisplayFormat(Temp);
	SDL_FreeSurface(Temp);
	SDL_FillRect(newinfo->InputBackground, NULL, SDL_MapRGBA(newinfo->ConsoleSurface->format, 0, 0, 0, newinfo->ConsoleAlpha));

	CON_Out(newinfo, "Console initialised.");
	CON_ListCommands(newinfo);

	return newinfo;
}

/* Frees all the memory loaded by the console */
void CON_Destroy(ConsoleInformation *console)
{
	int i;


	CON_DestroyCommands();
	DT_DestroyDrawText();
	for(i = 0; i <= console->Line_Buffer - 1; i++)
	{
		free(console->ConsoleLines[i]);
		free(console->CommandLines[i]);
	}
	free(console->ConsoleLines);
	free(console->CommandLines);

	console->ConsoleLines = NULL;
	console->CommandLines = NULL;
}


/* Increments the console line */
void CON_NewLineConsole(ConsoleInformation *console)
{
	int loop;
	char *temp = console->ConsoleLines[console->Line_Buffer - 1];


	for(loop = console->Line_Buffer - 1; loop > 1; loop--)
		console->ConsoleLines[loop] = console->ConsoleLines[loop - 1];

	console->ConsoleLines[1] = temp;

	memset(console->ConsoleLines[1], 0, CON_CHARS_PER_LINE);
	if(console->TotalConsoleLines < console->Line_Buffer - 1)
		console->TotalConsoleLines++;
}


/* Increments the command lines */
void CON_NewLineCommand(ConsoleInformation *console)
{
	int loop;
	char *temp = console->CommandLines[console->Line_Buffer - 1];


	for(loop = console->Line_Buffer - 1; loop > 0; loop--)
		console->CommandLines[loop] = console->CommandLines[loop - 1];

	console->CommandLines[0] = temp;

	memset(console->CommandLines[0], 0, CON_CHARS_PER_LINE);
	if(console->TotalCommands < console->Line_Buffer - 1)
		console->TotalCommands++;
}

/* Draws the command line the user is typing in to the screen */
static void DrawCommandLine()
{
	SDL_Rect rect, rect2;
	char temp[CON_CHARS_PER_LINE];
	BitFont *CurrentFont = DT_FontPointer(Topmost->FontNumber);
	static Uint32 LastBlinkTime = 0;	/* Last time the consoles cursor blinked */
	static int Blink = 0;			/* Is the cursor currently blinking */


	/* Check if the blink period is over */
	if(SDL_GetTicks() > LastBlinkTime)
	{
		LastBlinkTime = SDL_GetTicks() + CON_BLINK_RATE;

		if(Blink)
		{
			Blink = 0;
			/* The line was being drawn before, now it must be blacked out. */
			rect.x = strlen(Topmost->ConsoleLines[0]) * DT_FontWidth(Topmost->FontNumber) + CON_CHAR_BORDER;
			rect.y = Topmost->ConsoleSurface->h - DT_FontHeight(Topmost->FontNumber);
			rect.w = DT_FontWidth(Topmost->FontNumber);
			rect.h = DT_FontHeight(Topmost->FontNumber);
			SDL_FillRect(Topmost->ConsoleSurface, &rect, SDL_MapRGBA(Topmost->ConsoleSurface->format, 0, 0, 0, Topmost->ConsoleAlpha));
			/* Now draw the background image if applicable */
			if(Topmost->BackgroundImage)
			{
				rect2.x = strlen(Topmost->ConsoleLines[0]) * DT_FontWidth(Topmost->FontNumber) + CON_CHAR_BORDER;
				rect.x = rect2.x - Topmost->BackX;
				rect2.y = Topmost->ConsoleSurface->h - DT_FontHeight(Topmost->FontNumber);
				rect.y = rect2.y - Topmost->BackY;
				rect2.w = rect.w = DT_FontWidth(Topmost->FontNumber);
				rect2.h = rect.h = DT_FontHeight(Topmost->FontNumber);
				SDL_BlitSurface(Topmost->BackgroundImage, &rect, Topmost->ConsoleSurface, &rect2);
			}
		}
		else
			Blink = 1;
	}

	/* If there is enough buffer space add a cursor if it's time to Topmost->Blink '_' */
	/* once again we're drawing text, so in OpenGL context we need to temporarily set up
	 * software-mode transparency.
	 */
	if(Topmost->OutputScreen->flags & SDL_OPENGLBLIT)
	{
		Uint32 *pix = (Uint32 *) (CurrentFont->FontSurface->pixels);
		SDL_SetColorKey(CurrentFont->FontSurface, SDL_SRCCOLORKEY, *pix);
	}
	if(Blink && strlen(Topmost->ConsoleLines[0]) + 1 < CON_CHARS_PER_LINE)
	{
		strcpy(temp, Topmost->ConsoleLines[0]);
		temp[strlen(Topmost->ConsoleLines[0])] = '_';
		temp[strlen(Topmost->ConsoleLines[0]) + 1] = '\0';
		DT_DrawText(temp, Topmost->ConsoleSurface, Topmost->FontNumber, CON_CHAR_BORDER, Topmost->ConsoleSurface->h - DT_FontHeight(Topmost->FontNumber));
	}
	else	/* Not time to blink or the strings too long, just draw it. */
	{
		DT_DrawText(Topmost->ConsoleLines[0], Topmost->ConsoleSurface, Topmost->FontNumber, CON_CHAR_BORDER, Topmost->ConsoleSurface->h - DT_FontHeight(Topmost->FontNumber));
	}
	if(Topmost->OutputScreen->flags & SDL_OPENGLBLIT)
	{
		SDL_SetColorKey(CurrentFont->FontSurface, 0, 0);
	}
}

/* Outputs text to the console (in game and stdout), up to 256 chars can be entered */
void CON_Out(ConsoleInformation *console, const char *str, ...)
{
	va_list marker;
	char temp[256];


	va_start(marker, str);
	vsprintf(temp, str, marker);
	va_end(marker);

	if(console->ConsoleLines)
	{
		strncpy(console->ConsoleLines[1], temp, CON_CHARS_PER_LINE);
		console->ConsoleLines[1][CON_CHARS_PER_LINE - 1] = '\0';
		CON_NewLineConsole(console);
		CON_UpdateConsole(console);
	}

	/* And print to stdout */
	printf("%s\n", temp);
}


/* Sets the alpha level of the console, 0 turns off alpha blending */
void CON_Alpha(ConsoleInformation *console, unsigned char alpha)
{
	/* store alpha as state! */
	console->ConsoleAlpha = alpha;

	if((console->OutputScreen->flags & SDL_OPENGLBLIT) == 0)
		if(alpha == 0)
			SDL_SetAlpha(console->ConsoleSurface, 0, alpha);
		else
			SDL_SetAlpha(console->ConsoleSurface, SDL_SRCALPHA, alpha);

	CON_UpdateConsole(console);
}


/* Adds  background image to the console, x and y based on consoles x and y */
int CON_Background(ConsoleInformation *console, const char *image, int x, int y)
{
	SDL_Surface *temp;
	SDL_Rect backgroundsrc, backgrounddest;


	/* Free the background from the console */
	if(image == NULL && console->BackgroundImage != NULL)
	{
		SDL_FreeSurface(console->BackgroundImage);
		console->BackgroundImage = NULL;
		SDL_FillRect(console->InputBackground, NULL, SDL_MapRGBA(console->ConsoleSurface->format, 0, 0, 0, SDL_ALPHA_OPAQUE));
		return 0;
	}

	/* Load a new background */
	if(NULL == (temp = SDL_LoadBMP(image)))
	{
		CON_Out(console, "Cannot load background %s.", image);
		return 1;
	}

	/* Remove the existing background if it's there. */
	if(console->BackgroundImage != NULL)
		SDL_FreeSurface(console->BackgroundImage);


	console->BackgroundImage = SDL_DisplayFormat(temp);
	SDL_FreeSurface(temp);
	console->BackX = x;
	console->BackY = y;

	backgroundsrc.x = 0;
	backgroundsrc.y = console->ConsoleSurface->h - DT_FontHeight(console->FontNumber) - console->BackY;
	backgroundsrc.w = console->BackgroundImage->w;
	backgroundsrc.h = console->InputBackground->h;

	backgrounddest.x = console->BackX;
	backgrounddest.y = 0;
	backgrounddest.w = console->BackgroundImage->w;
	backgrounddest.h = DT_FontHeight(console->FontNumber);

	SDL_FillRect(console->InputBackground, NULL, SDL_MapRGBA(console->ConsoleSurface->format, 0, 0, 0, SDL_ALPHA_OPAQUE));
	SDL_BlitSurface(console->BackgroundImage, &backgroundsrc, console->InputBackground, &backgrounddest);

	return 0;
}

/* takes a new x and y of the top left of the console window */
void CON_Position(ConsoleInformation *console, int x, int y)
{
	if(x < 0 || x > console->OutputScreen->w - console->ConsoleSurface->w)
		console->DispX = 0;
	else
		console->DispX = x;

	if(y < 0 || y > console->OutputScreen->h - console->ConsoleSurface->h)
		console->DispY = 0;
	else
		console->DispY = y;
}

/* resizes the console, has to reset alot of stuff
 * returns 1 on error */
int CON_Resize(ConsoleInformation *console, SDL_Rect rect)
{
	SDL_Surface *Temp;
	SDL_Rect backgroundsrc, backgrounddest;


	/* make sure that the size of the console is valid */
	if(rect.w > console->OutputScreen->w || rect.w < DT_FontWidth(console->FontNumber) * 32)
		rect.w = console->OutputScreen->w;
	if(rect.h > console->OutputScreen->h || rect.h < DT_FontHeight(console->FontNumber))
		rect.h = console->OutputScreen->h;
	if(rect.x < 0 || rect.x > console->OutputScreen->w - rect.w)
		console->DispX = 0;
	else
		console->DispX = rect.x;
	if(rect.y < 0 || rect.y > console->OutputScreen->h - rect.h)
		console->DispY = 0;
	else
		console->DispY = rect.y;

	/* load the console surface */
	SDL_FreeSurface(console->ConsoleSurface);
	Temp = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, console->OutputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if(Temp == NULL)
	{
		PRINT_ERROR("Couldn't create the console->ConsoleSurface\n");
		return 1;
	}
	console->ConsoleSurface = SDL_DisplayFormat(Temp);
	SDL_FreeSurface(Temp);

	/* Load the dirty rectangle for user input */
	SDL_FreeSurface(console->InputBackground);
	Temp = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, DT_FontHeight(console->FontNumber), console->OutputScreen->format->BitsPerPixel, 0, 0, 0, 0);
	if(Temp == NULL)
	{
		PRINT_ERROR("Couldn't create the input background\n");
		return 1;
	}
	console->InputBackground = SDL_DisplayFormat(Temp);
	SDL_FreeSurface(Temp);

	/* Now reset some stuff dependent on the previous size */
	console->ConsoleScrollBack = 0;

	/* Reload the background image (for the input text area) in the console */
	if(console->BackgroundImage)
	{
		backgroundsrc.x = 0;
		backgroundsrc.y = console->ConsoleSurface->h - DT_FontHeight(console->FontNumber) - console->BackY;
		backgroundsrc.w = console->BackgroundImage->w;
		backgroundsrc.h = console->InputBackground->h;

		backgrounddest.x = console->BackX;
		backgrounddest.y = 0;
		backgrounddest.w = console->BackgroundImage->w;
		backgrounddest.h = DT_FontHeight(console->FontNumber);

		SDL_FillRect(console->InputBackground, NULL, SDL_MapRGBA(console->ConsoleSurface->format, 0, 0, 0, SDL_ALPHA_OPAQUE));
		SDL_BlitSurface(console->BackgroundImage, &backgroundsrc, console->InputBackground, &backgrounddest);
	}

	return 0;
}

/* Sets the topmost console for input */
void CON_Topmost(ConsoleInformation *console)
{
	SDL_Rect	rect, rect2;


	/* Make sure the blinking cursor is gone */
	if(Topmost != NULL)
	{
		rect.x = strlen(Topmost->ConsoleLines[0]) * DT_FontWidth(Topmost->FontNumber) + CON_CHAR_BORDER;
		rect.y = Topmost->ConsoleSurface->h - DT_FontHeight(Topmost->FontNumber);
		rect.w = DT_FontWidth(Topmost->FontNumber);
		rect.h = DT_FontHeight(Topmost->FontNumber);
		SDL_FillRect(Topmost->ConsoleSurface, &rect, SDL_MapRGBA(Topmost->ConsoleSurface->format, 0, 0, 0, Topmost->ConsoleAlpha));
		/* Now draw the background image if applicable */
		if(Topmost->BackgroundImage)
		{
			rect2.x = strlen(Topmost->ConsoleLines[0]) * DT_FontWidth(Topmost->FontNumber) + CON_CHAR_BORDER;
			rect.x = rect2.x - Topmost->BackX;
			rect2.y = Topmost->ConsoleSurface->h - DT_FontHeight(Topmost->FontNumber);
			rect.y = rect2.y - Topmost->BackY;
			rect2.w = rect.w = DT_FontWidth(Topmost->FontNumber);
			rect2.h = rect.h = DT_FontHeight(Topmost->FontNumber);
			SDL_BlitSurface(Topmost->BackgroundImage, &rect, Topmost->ConsoleSurface, &rect2);
		}
	}

	Topmost = console;
}
