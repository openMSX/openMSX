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

struct osd_bitmap
{
	int width,height;       /* width and height of the bitmap */
	int depth;              /* bits per pixel */
	Pixel *_private;        /* don't touch! - reserved for osdepend use */
	Pixel **line;           /* pointers to the start of each line */
	// Here the differences from the MAME structure starts
	int safety;
	int safetx;
};

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
	static int debugColor;		// debug
	Emutime currentTime;

	SDL_Surface *screen;
	struct {
		/* TMS9928A internal settings */
		byte ReadAhead,Regs[8],StatusReg,oldStatusReg;
		int Addr,FirstByte,BackColour,Change,mode;
		int colour,pattern,nametbl,spriteattribute,spritepattern;
		int colourmask,patternmask;
		/* memory */
		byte *vMem, *dBackMem;
		int vramsize, model;
		/* emulation settings */
		int LimitSprites; /* max 4 sprites on a row, like original TMS9918A */
		/* all or nothing dirty David Heremans */
		bool stateChanged;
		/* dirty tables from Sean Young restcode */
		byte anyDirtyColour, anyDirtyName, anyDirtyPattern;
		byte *DirtyColour, *DirtyName, *DirtyPattern;
	} tms;
	byte TMS9928A_vram_r();

	Pixel XPal[16];
	void PutImage();
	struct osd_bitmap *bitmapscreen;

	struct osd_bitmap *alloc_bitmap(int width, int height, int depth);
	void _TMS9928A_change_register (byte reg, byte val);
	void _TMS9928A_set_dirty (char);

	void fullScreenRefresh();
	void full_border_fil();

	void mode0(struct osd_bitmap*);
	void mode1(struct osd_bitmap*);
	void mode2(struct osd_bitmap*);
	void mode12(struct osd_bitmap*);
	void mode3(struct osd_bitmap*);
	void modebogus(struct osd_bitmap*);
	void mode23(struct osd_bitmap*);
	void modeblank(struct osd_bitmap*);
	void sprites(struct osd_bitmap*);

};
#endif //___MSXTMS9928A_HH__

