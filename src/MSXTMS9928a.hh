// $Id$

#ifndef __MSXTMS9928A_HH__
#define __MSXTMS9928A_HH__

//#include <iostream>
//#include <fstream>
#include <SDL/SDL.h>

#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"

#define WIDTH 320
#define HEIGHT 240
#define DEPTH 8

#if DEPTH == 8
typedef byte Pixel;
#elif DEPTH == 15 || DEPTH == 16
typedef word Pixel;
#elif DEPTH == 32
typedef unsigned int Pixel;
#else
#error DEPTH must be 8, 15, 16 or 32 bits per pixel.
#endif

class MSXTMS9928a : public MSXDevice
{
public:
	//constructor and destructor
	MSXTMS9928a(void);
	~MSXTMS9928a(void);

	// interaction with CPU
	byte readIO(byte port, Emutime &time);
	void writeIO(byte port, byte value, Emutime &time);
	void executeUntilEmuTime(const Emutime &time);

	// mainlife cycle of an MSXDevice
	void init();
	void start();
	//void stop();
	void reset();

	// void saveState(ofstream writestream);
	// void restoreState(char *devicestring,ifstream readstream);

private:
	typedef void (MSXTMS9928a::*RenderMethod)(Pixel *pixelPtr, int line);
	static RenderMethod modeToRenderMethod[];

	/** Limit number of sprites per display line?
	  * Option only affects display, not MSX state.
	  * In other words: when off all sprites are drawn,
	  * but status registers act like they aren't.
	  */
	bool limitSprites;

	Emutime currentTime;

	SDL_Surface *screen;
	struct {
		/* TMS9928A internal settings */
		byte ReadAhead,Regs[8],StatusReg,oldStatusReg;
		int Addr,FirstByte,BackColour,Change,mode;
		int colour,pattern,nametbl,spriteattribute,spritepattern;
		int colourmask,patternmask;
		/* memory */
		byte *vMem;
		int vramsize, model;
	} tms;

	byte TMS9928A_vram_r();

	Pixel XPal[16];
	Pixel currBorderColours[HEIGHT];

	/** Put an image on the screen.
	  */
	void putImage();
	void _TMS9928A_change_register(byte reg, byte val);
	/** Set all dirty / clean.
	  */
	void setDirty(bool);

	void fullScreenRefresh();

	void mode0(Pixel *pixelPtr, int line);
	void mode1(Pixel *pixelPtr, int line);
	void mode2(Pixel *pixelPtr, int line);
	void mode12(Pixel *pixelPtr, int line);
	void mode3(Pixel *pixelPtr, int line);
	void modebogus(Pixel *pixelPtr, int line);
	void mode23(Pixel *pixelPtr, int line);
	void modeblank(Pixel *pixelPtr, int line);

	/** Draw sprites on this line over the background.
	  * @param dirty 32-entry array that stores which characters are
	  *   covered with sprites and must therefore be redrawn next frame.
	  *   This method will update the array according to the sprites drawn.
	  * @return Where any pixels drawn?
	  */
	bool drawSprites(Pixel *pixelPtr, int line, bool *dirty);
	/** Check sprite collision and number of sprites per line.
	  * Separated from display code to make MSX behaviour consistent
	  * no matter how displaying is handled.
	  * @param patterns Pointer to a 4-entry int array in which the
	  *   numbers of the sprites to be displayed are returned.
	  *   If null, no such array is returned.
	  * @return The number of sprites stored in the visibleSprites array.
	  *   Undefined if no such array was given.
	  */
	int checkSprites(int line, int *visibleSprites = 0);
	void sprites(Pixel *linePtrs[], int displayX);

	/* emulation settings */
	/* all or nothing dirty David Heremans */
	bool stateChanged;
	/* dirty tables from Sean Young restcode */
	bool anyDirtyColour, dirtyColour[256 * 3];
	bool anyDirtyPattern, dirtyPattern[256 * 3];
	bool anyDirtyName, dirtyName[40 * 24];

	/** Actual pixel data.
	  */
	Pixel pixelData[WIDTH * HEIGHT];
	/** Pointers to the start of each line.
	  */
	Pixel *linePtrs[HEIGHT];
};
#endif //___MSXTMS9928A_HH__

