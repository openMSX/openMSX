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


struct osd_bitmap
{
	int width,height;       /* width and height of the bitmap */
	int depth;                      /* bits per pixel */
	byte *_private;         /* don't touch! - reserved for osdepend use */
	byte  **line;           /* pointers to the start of each line */
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
		void writeIO(byte port,byte value, Emutime &time);
		void executeUntilEmuTime(const Emutime &time);
		
		// mainlife cycle of an MSXDevice
		void init();
		void start();
		//void stop();
		void reset();
		
		// void saveState(ofstream writestream);
		// void restoreState(char *devicestring,ifstream readstream);
	
		void mode0(struct osd_bitmap*);
		void mode1(struct osd_bitmap*);
		void mode2(struct osd_bitmap*);
		void mode12(struct osd_bitmap*);
		void mode3(struct osd_bitmap*);
		void modebogus(struct osd_bitmap*);
		void mode23(struct osd_bitmap*);

	private:
		static int debugColor;		// debug
		Emutime currentTime;

	SDL_Surface *screen;
        struct {
          /* TMS9928A internal settings */
          byte ReadAhead,Regs[8],StatusReg,oldStatusReg;
          int Addr,FirstByte,BackColour,Change,mode;
	  //int INT;
          int colour,pattern,nametbl,spriteattribute,spritepattern;
          int colourmask,patternmask;
          //void (*INTCallback)(int);
          /* memory */
          byte *vMem, *dBackMem;
          struct osd_bitmap *tmpbmp;
          int vramsize, model;
          /* emulation settings */
          int LimitSprites; /* max 4 sprites on a row, like original TMS9918A */
          /* dirty tables */
          byte anyDirtyColour, anyDirtyName, anyDirtyPattern;
          byte *DirtyColour, *DirtyName, *DirtyPattern;
        } tms;
        byte TMS9928A_vram_r();
	
        unsigned int XPal[20];
        void PutImage();
        struct osd_bitmap *bitmapscreen;

        void plot_pixel(struct osd_bitmap *bitmap,int x,int y,int pen);
        struct osd_bitmap* alloc_bitmap(int width,int height,int depth);
        void _TMS9928A_change_register (byte reg, byte val);

	void fullScreenRefresh();
	void full_border_fil();
};
#endif //___MSXTMS9928A_HH__

