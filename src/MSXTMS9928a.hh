// $Id$

#ifndef __MSXTMS9928A_HH__
#define __MSXTMS9928A_HH__

#include <iostream.h>
#include <fstream.h>
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
	void *_private;         /* don't touch! - reserved for osdepend use */
	byte  **line;           /* pointers to the start of each line */
};

class MSXTMS9928a : public MSXDevice
{
	public:
		//destructor and 
		MSXTMS9928a(void);
		~MSXTMS9928a(void);
		//
		//instantiate method used in DeviceFactory
		//Must be overwritten in derived classes !!!!
		//void setConfigDevice(MSXConfig::Device *config);
		
		// interaction with CPU
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port,byte value, Emutime &time);
		void executeUntilEmuTime(Emutime &time);
		// int executeTStates(int TStates);
		// int getUsedTStates(void);
		//
		// mainlife cycle of an MSXDevice
		void init();
		void start();
		//void stop();
		void reset();
		//
		// void saveState(ofstream writestream);
		// void restoreState(char *devicestring,ifstream readstream);
	//protected:
		//MSXConfig::Device *deviceConfig;
		//These are used for save/restoreState see note over
		//savefile-structure
		// To ease the burden of keeping IRQ state
		//bool isIRQset;
		//void setInterrupt();
        //void resetInterrupt();
    private:
	Emutime currentTime;

	SDL_Surface *screen;
        struct {
          /* TMS9928A internal settings */
          byte ReadAhead,Regs[8],StatusReg,oldStatusReg;
          int Addr,FirstByte,INT,BackColour,Change,mode;
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
};
#endif //___MSXTMS9928A_HH__

