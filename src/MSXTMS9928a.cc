/*
** File: tms9928a.c -- software implementation of the Texas Instruments
**                     TMS9918A and TMS9928A, used by the Coleco, MSX and
**                     TI99/4A.
**
** All undocumented features as described in the following file
** should be emulated.
**
** http://www.msxnet.org/tech/tms9918a.txt
**
** By Sean Young 1999 (sean@msxnet.org).
** Based on code by Mike Balfour. Features added:
** - read-ahead
** - single read/write address
** - AND mask for mode 2
** - multicolor mode
** - undocumented screen modes
** - illegal sprites (max 4 on one line)
** - vertical coordinate corrected -- was one to high (255 => 0, 0 => 1)
** - errors in interrupt emulation
** - back drop correctly emulated.
**
** 19 feb 2000, Sean:
** - now uses plot_pixel (..), so -ror works properly
** - fixed bug in tms.patternmask
**
** 3 nov 2000, Raphael Nabet:
** - fixed a nasty bug in _TMS9928A_sprites. A transparent sprite caused 
**   sprites at lower levels not to be displayed, which is wrong.
**
** 3 jan 2001, Sean Young:
** - A few minor cleanups
** - Changed TMS9928A_vram_[rw] and  TMS9928A_register_[rw] to READ_HANDLER 
**   and WRITE_HANDLER.
** - Got rid of the color table, unused. Also got rid of the old colors,
**   which where commented out anyways.
** 
**
** Todo:
** - The screen image is rendered in `one go'. Modifications during
**   screen build up are not shown.
** - Correctly emulate 4,8,16 kb VRAM if needed.
** - uses plot_pixel (...) in TMS_sprites (...), which is rended in
**   in a back buffer created with malloc (). Hmm..
** - Colours are incorrect. [fixed by R Nabet ?]
*/

/*
** 12/08/2001
** David Heremans started rewritting the existing code to make it a
** MSXDevice for the openMSX emulator.
** 13/08/2001 did to readIO melting of READ_HANDLER
** 15/08/2001 This doesn't go as planned, converting it all at once is
** nonsens. I'm chancing tactics I remove everything, that I don't need 
** then restart with alloc_bitmap, then try to craeate a simple window
** and finally import all mode functions one by one
*/

//#include "driver.h"
//#include "vidhrdw/generic.h"
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXTMS9928a.hh"
#include <assert.h>

/*
	New palette (R. Nabet).

	First 3 columns from TI datasheet (in volts).
	Next 3 columns based on formula :
		Y = .299*R + .587*G + .114*B (NTSC)
	(the coefficients are likely to be slightly different with PAL, but who cares ?)
	I assumed the "zero" for R-Y and B-Y was 0.47V.
	Last 3 coeffs are the 8-bit values.

	Color            Y  	R-Y 	B-Y 	R   	G   	B   	R	G	B
	0 Transparent
	1 Black         0.00	0.47	0.47	0.00	0.00	0.00	  0	  0	  0
	2 Medium green  0.53	0.07	0.20	0.13	0.79	0.26	 33	200	 66
	3 Light green   0.67	0.17	0.27	0.37	0.86	0.47	 94	220	120
	4 Dark blue     0.40	0.40	1.00	0.33	0.33	0.93	 84	 85	237
	5 Light blue    0.53	0.43	0.93	0.49	0.46	0.99	125	118	252
	6 Dark red      0.47	0.83	0.30	0.83	0.32	0.30	212	 82	 77
	7 Cyan          0.73	0.00	0.70	0.26	0.92	0.96	 66	235	245
	8 Medium red    0.53	0.93	0.27	0.99	0.33	0.33	252	 85	 84
	9 Light red     0.67	0.93	0.27	1.13(!)	0.47	0.47	255	121	120
	A Dark yellow   0.73	0.57	0.07	0.83	0.76	0.33	212	193	 84
	B Light yellow  0.80	0.57	0.17	0.90	0.81	0.50	230	206	128
	C Dark green    0.47	0.13	0.23	0.13	0.69	0.23	 33	176	 59
	D Magenta       0.53	0.73	0.67	0.79	0.36	0.73	201	 91	186
	E Gray          0.80	0.47	0.47	0.80	0.80	0.80	204	204	204
	F White         1.00	0.47	0.47	1.00	1.00	1.00	255	255	255
*/
static unsigned char TMS9928A_palette[16*3] =
{
	0, 0, 0,
	0, 0, 0,
	33, 200, 66,
	94, 220, 120,
	84, 85, 237,
	125, 118, 252,
	212, 82, 77,
	66, 235, 245,
	252, 85, 84,
	255, 121, 120,
	212, 193, 84,
	230, 206, 128,
	33, 176, 59,
	201, 91, 186,
	204, 204, 204,
	255, 255, 255
};

/*
** Defines for `dirty' optimization
*/

#define MAX_DIRTY_COLOUR        (256*3)
#define MAX_DIRTY_PATTERN       (256*3)
#define MAX_DIRTY_NAME          (40*24)

/*
** Forward declarations of internal functions.
**
**static void _TMS9928A_mode0 (struct osd_bitmap*);
**static void _TMS9928A_mode1 (struct osd_bitmap*);
**static void _TMS9928A_mode2 (struct osd_bitmap*);
**static void _TMS9928A_mode12 (struct osd_bitmap*);
**static void _TMS9928A_mode3 (struct osd_bitmap*);
**static void _TMS9928A_mode23 (struct osd_bitmap*);
**static void _TMS9928A_modebogus (struct osd_bitmap*);
**static void _TMS9928A_sprites (struct osd_bitmap*);
**static void _TMS9928A_change_register (int, UINT8);
**static void _TMS9928A_set_dirty (char);
**
**static void (*ModeHandlers[])(struct osd_bitmap*) = {
**        _TMS9928A_mode0, _TMS9928A_mode1, _TMS9928A_mode2,  _TMS9928A_mode12,
**        _TMS9928A_mode3, _TMS9928A_modebogus, _TMS9928A_mode23,
**        _TMS9928A_modebogus };
*/
#define IMAGE_SIZE (256*192)        /* size of rendered image        */

#define TMS_SPRITES_ENABLED ((tms.Regs[1] & 0x50) == 0x40)
#define TMS_MODE ( (tms.model == TMS99x8A ? (tms.Regs[0] & 2) : 0) | \
	((tms.Regs[1] & 0x10)>>4) | ((tms.Regs[1] & 8)>>1))

/*
** initialize the palette 
*/
//void tms9928A_init_palette (unsigned char *palette, 
//	unsigned short *colortable,const unsigned char *color_prom) {
//    memcpy (palette, &TMS9928A_palette, sizeof(TMS9928A_palette));
//}

MSXTMS9928a::MSXTMS9928a() : currentTime(28636360, 0)	// TODO check freq
{
    cout << "Creating an MSXTMS9928a object \n";
}
MSXTMS9928a::~MSXTMS9928a()
{
    cout << "Destroying an MSXTMS9928a object \n";
}

/*
** The init, start, reset and shutdown functions
*/
void MSXTMS9928a::reset ()
{
    int  i;

    for (i=0;i<8;i++) tms.Regs[i] = 0;
    tms.StatusReg = 0;
    tms.nametbl = tms.pattern = tms.colour = 0;
    tms.spritepattern = tms.spriteattribute = 0;
    tms.colourmask = tms.patternmask = 0;
    tms.Addr = tms.ReadAhead = tms.INT = 0;
    tms.mode = tms.BackColour = 0;
    tms.Change = 1;
    tms.FirstByte = -1;
    //_TMS9928A_set_dirty (1);
}

//int TMS9928A_start (int model, unsigned int vram) 
void MSXTMS9928a::init(void)
{
    // /* 4 or 16 kB vram please */
    // if (! ((vram == 0x1000) || (vram == 0x4000) || (vram == 0x2000)) )
    //    return 1;

    //tms.model = model;
    tms.model = 1;	//tms9928a

    bitmapscreen= alloc_bitmap (256,192 , 8);
    if (!bitmapscreen) return ;//(1);

    /* Video RAM */
    tms.vramsize = 0x4000;
    tms.vMem = new byte [0x4000]; //(UINT8*) malloc (vram);
    if (!tms.vMem) return ;//(1);
    memset (tms.vMem, 0, 0x4000);   // memset (tms.vMem, 0, vram);

    /* Sprite back buffer */
    tms.dBackMem = new byte [IMAGE_SIZE]; //(UINT8*)malloc (IMAGE_SIZE);
    if (!tms.dBackMem) {
        free (tms.vMem);
        return ;//1;
    }

    /* dirty buffers */
    tms.DirtyName =  new byte [MAX_DIRTY_NAME]; // (char*)malloc (MAX_DIRTY_NAME);
    if (!tms.DirtyName) {
        free (tms.vMem);
        free (tms.dBackMem);
        return ;//1;
    }

    tms.DirtyPattern = new byte [MAX_DIRTY_PATTERN]; // (char*)malloc (MAX_DIRTY_PATTERN);
    if (!tms.DirtyPattern) {
        free (tms.vMem);
        free (tms.DirtyName);
        free (tms.dBackMem);
        return ;//1;
    }

    tms.DirtyColour =  new byte [MAX_DIRTY_COLOUR]; // (char*)malloc (MAX_DIRTY_COLOUR);
    if (!tms.DirtyColour) {
        free (tms.vMem);
        free (tms.DirtyName);
        free (tms.DirtyPattern);
        free (tms.dBackMem);
        return ;//1;
    }

    /* back bitmap */
    tms.tmpbmp = alloc_bitmap (256, 192, 8);
    if (!tms.tmpbmp) {
        free (tms.vMem);
        free (tms.dBackMem);
        free (tms.DirtyName);
        free (tms.DirtyPattern);
        free (tms.DirtyColour);
        return ;//1;
    }

    reset() ; // TMS9928A_reset ();
    tms.LimitSprites = 1;
  
  
  /* Open the display */
    //if(Verbose) printf("OK\n  Opening display...");
    printf("OK\n  Opening display...");
    #ifdef FULLSCREEN
      if ((screen=SDL_SetVideoMode(WIDTH,HEIGHT,8,SDL_SWSURFACE|SDL_FULLSCREEN))==NULL)
    #else
      if ((screen=SDL_SetVideoMode(WIDTH,HEIGHT,8,SDL_SWSURFACE))==NULL)
    #endif
	{ printf("FAILED");return; }
	//{ if(Verbose) printf("FAILED");return; }

     /* Hide mouse cursor */
       SDL_ShowCursor(0);

  /* Init image buffer */
//    XBuf=malloc(WIDTH*HEIGHT*sizeof(pixel));
//    memset(XBuf,0,WIDTH*HEIGHT*sizeof(pixel));

     /* Reset the palette */
     for(int J=0;J<16;J++){
         XPal[J]=SDL_MapRGB(screen->format,
	 		TMS9928A_palette[J*3],
			TMS9928A_palette[J*3 + 1],
			TMS9928A_palette[J*3 + 2]);
     };

     //  /* Set SCREEN8 colors */
     //  for(J=0;J<64;J++)
     //  {
     //    I=(J&0x03)+(J&0x0C)*16+(J&0x30)/2;
     //    XPal[J+16]=SDL_MapRGB(screen->format,
     //>·····>·······>·······  (J>>4)*255/3,((J>>2)&0x03)*255/3,(J&0x03)*255/3);
     //    BPal[I]=BPal[I|0x04]=BPal[I|0x20]=BPal[I|0x24]=XPal[J+16];
     //  }

     MSXMotherBoard::instance()->register_IO_In((byte)0x98,this);
     MSXMotherBoard::instance()->register_IO_In((byte)0x99,this);
     MSXMotherBoard::instance()->register_IO_Out((byte)0x98,this);
     MSXMotherBoard::instance()->register_IO_Out((byte)0x99,this);

	//debug code
	for (byte i=0; i<190 ;i++) 
		plot_pixel(bitmapscreen ,i,i,XPal[4]);
    return ;//0;
}

void MSXTMS9928a::start()
{
	//First interrupt in Pal mode here
	MSXMotherBoard::instance()->insertStamp(currentTime+71285, *this);
	PutImage();
}

void MSXTMS9928a::executeUntilEmuTime(Emutime &time)
{
  //Next interrupt in Pal mode here
  MSXMotherBoard::instance()->insertStamp(currentTime+71285, *this);

  //TODO:: Call full screen refresh
  //TODO:: Set interrupt if bits enable it
  PutImage();
}

/*
~MSXTMS9928a()
{
    free (tms.vMem); tms.vMem = NULL;
    free (tms.dBackMem); tms.dBackMem = NULL;
    free (tms.DirtyColour); tms.DirtyColour = NULL;
    free (tms.DirtyName); tms.DirtyName = NULL;
    free (tms.DirtyPattern); tms.DirtyPattern = NULL;
    osd_free_bitmap (tms.tmpbmp); tms.tmpbmp = NULL;
}
*/

/*
** Set all dirty / clean
*
static void _TMS9928A_set_dirty (char dirty) {
    tms.anyDirtyColour = tms.anyDirtyName = tms.anyDirtyPattern = dirty;
    memset (tms.DirtyName, dirty, MAX_DIRTY_NAME);
    memset (tms.DirtyColour, dirty, MAX_DIRTY_COLOUR);
    memset (tms.DirtyPattern, dirty, MAX_DIRTY_PATTERN);
}
*/

/*
** The I/O functions.
*/


void MSXTMS9928a::writeIO(byte port, byte value, Emutime &time)
{
  int i;
  switch (port){
    case 0x98:
      //WRITE_HANDLER (TMS9928A_vram_w)
      if (tms.vMem[tms.Addr] != value) {
	tms.vMem[tms.Addr] = value;
	tms.Change = 1;
	/* dirty optimization */
	if ( (tms.Addr >= tms.nametbl) &&
	    (tms.Addr < (tms.nametbl + MAX_DIRTY_NAME) ) ) {
	  tms.DirtyName[tms.Addr - tms.nametbl] = 1;
	  tms.anyDirtyName = 1;
	}

	i = (tms.Addr - tms.colour) >> 3;
	if ( (i >= 0) && (i < MAX_DIRTY_COLOUR) ) {
	  tms.DirtyColour[i] = 1;
	  tms.anyDirtyColour = 1;
	}

	i = (tms.Addr - tms.pattern) >> 3;
	if ( (i >= 0) && (i < MAX_DIRTY_PATTERN) ) {
	  tms.DirtyPattern[i] = 1;
	  tms.anyDirtyPattern = 1;
	}
      }
      tms.Addr = (tms.Addr + 1) & (tms.vramsize - 1);
      tms.ReadAhead = value;
      tms.FirstByte = -1;
      break;
    case 0x99:
      //WRITE_HANDLER (TMS9928A_register_w)
      if (tms.FirstByte >= 0) {
	if (value & 0x80) {
	  /* register write */
	  _TMS9928A_change_register ((int)(value & 7), tms.FirstByte);
	} else {
	  /* set read/write address */
	  tms.Addr = ((word)value << 8 | tms.FirstByte) & (tms.vramsize - 1);
	  if ( !(value & 0x40) ) {
	    /* read ahead */
	    TMS9928A_vram_r();
	  }
	}
	tms.FirstByte = -1;
      } else {
	tms.FirstByte = value;
      }
      break;
  }
}

byte MSXTMS9928a::TMS9928A_vram_r()
{
      //READ_HANDLER (TMS9928A_vram_r) 
      byte b;
      b = tms.ReadAhead;
      tms.ReadAhead = tms.vMem[tms.Addr];
      tms.Addr = (tms.Addr + 1) & (tms.vramsize - 1);
      tms.FirstByte = -1;
      return b;
}

byte MSXTMS9928a::readIO(byte port, Emutime &time)
{
	byte b;
	switch (port) {
	case 0x98:
		return TMS9928A_vram_r();
	case 0x99:
		//READ_HANDLER (TMS9928A_register_r) 
		b = tms.StatusReg;
		tms.StatusReg &= 0x5f;
		if (tms.INT) {
			tms.INT = 0;
			// TODO: if (tms.INTCallback) tms.INTCallback (tms.INT);
		}
		tms.FirstByte = -1;
		return b;
	default:
		assert(false);
	}
}


void MSXTMS9928a::_TMS9928A_change_register (byte reg, byte val) {
    static const byte Mask[8] =
        { 0x03, 0xfb, 0x0f, 0xff, 0x07, 0x7f, 0x07, 0xff };
    static const char *modes[] = {
        "Mode 0 (GRAPHIC 1)", "Mode 1 (TEXT 1)", "Mode 2 (GRAPHIC 2)",
        "Mode 1+2 (TEXT 1 variation)", "Mode 3 (MULTICOLOR)",
        "Mode 1+3 (BOGUS)", "Mode 2+3 (MULTICOLOR variation)",
        "Mode 1+2+3 (BOGUS)" };
    byte b,oldval;
    int mode;

    val &= Mask[reg];
    oldval = tms.Regs[reg];
    if (oldval == val) return;
    tms.Regs[reg] = val;

    //logerror("TMS9928A: Reg %d = %02xh\n", reg, (int)val);
    tms.Change = 1;
    /*
    switch (reg) {
    case 0:
        if ( (val ^ oldval) & 2) {
            // re-calculate masks and pattern generator & colour 
            if (val & 2) {
                tms.colour = ((tms.Regs[3] & 0x80) * 64) & (tms.vramsize - 1);
                tms.colourmask = (tms.Regs[3] & 0x7f) * 8 | 7;
                tms.pattern = ((tms.Regs[4] & 4) * 2048) & (tms.vramsize - 1);
                tms.patternmask = (tms.Regs[4] & 3) * 256 |
		    (tms.colourmask & 255);
            } else {
                tms.colour = (tms.Regs[3] * 64) & (tms.vramsize - 1);
                tms.pattern = (tms.Regs[4] * 2048) & (tms.vramsize - 1);
            }
            tms.mode = TMS_MODE;
            logerror("TMS9928A: %s\n", modes[tms.mode]);
            _TMS9928A_set_dirty (1);
        }
        break;
    case 1:
        // check for changes in the INT line
        b = (val & 0x20) && (tms.StatusReg & 0x80) ;
        if (b != tms.INT) {
            tms.INT = b;
            if (tms.INTCallback) tms.INTCallback (tms.INT);
        }
        mode = TMS_MODE;
        if (tms.mode != mode) {
            tms.mode = mode;
            _TMS9928A_set_dirty (1);
            logerror("TMS9928A: %s\n", modes[tms.mode]);
        }
        break;
    case 2:
        tms.nametbl = (val * 1024) & (tms.vramsize - 1);
        tms.anyDirtyName = 1;
        memset (tms.DirtyName, 1, MAX_DIRTY_NAME);
        break;
    case 3:
        if (tms.Regs[0] & 2) {
            tms.colour = ((val & 0x80) * 64) & (tms.vramsize - 1);
            tms.colourmask = (val & 0x7f) * 8 | 7;
         } else {
            tms.colour = (val * 64) & (tms.vramsize - 1);
        }
        tms.anyDirtyColour = 1;
        memset (tms.DirtyColour, 1, MAX_DIRTY_COLOUR);
        break;
    case 4:
        if (tms.Regs[0] & 2) {
            tms.pattern = ((val & 4) * 2048) & (tms.vramsize - 1);
            tms.patternmask = (val & 3) * 256 | 255;
        } else {
            tms.pattern = (val * 2048) & (tms.vramsize - 1);
        }
        tms.anyDirtyPattern = 1;
        memset (tms.DirtyPattern, 1, MAX_DIRTY_PATTERN);
        break;
    case 5:
        tms.spriteattribute = (val * 128) & (tms.vramsize - 1);
        break;
    case 6:
        tms.spritepattern = (val * 2048) & (tms.vramsize - 1);
        break;
    case 7:
        // The backdrop is updated at TMS9928A_refresh()
        tms.anyDirtyColour = 1;
        memset (tms.DirtyColour, 1, MAX_DIRTY_COLOUR);
        break;
    }
    */
}

/*
** Interface functions
*/

//void TMS9928A_int_callback (void (*callback)(int)) {
//    tms.INTCallback = callback;
//}

//void TMS9928A_set_spriteslimit (int limit) {
//    tms.LimitSprites = limit;
//}

/*
** Updates the screen (the dMem memory area).
*
void TMS9928A_refresh (struct osd_bitmap *bmp, int full_refresh) {
    int c;

    if (tms.Change) {
        c = tms.Regs[7] & 15; if (!c) c=1;
        if (tms.BackColour != c) {
            tms.BackColour = c;
            palette_change_color (0,
                TMS9928A_palette[c * 3], TMS9928A_palette[c * 3 + 1],
                TMS9928A_palette[c * 3 + 2]);
        }
    }

	if (palette_recalc() ) {
		_TMS9928A_set_dirty (1);
		tms.Change = 1;
	}

    if (tms.Change || full_refresh) {
        if (! (tms.Regs[1] & 0x40) ) {
            fillbitmap (bmp, Machine->pens[tms.BackColour],
                &Machine->visible_area);
        } else {
            if (tms.Change) ModeHandlers[tms.mode] (tms.tmpbmp);
            copybitmap (bmp, tms.tmpbmp, 0, 0, 0, 0,
                &Machine->visible_area, TRANSPARENCY_NONE, 0);
            if (TMS_SPRITES_ENABLED) {
                _TMS9928A_sprites (bmp);
            }
        }
    } else {
		tms.StatusReg = tms.oldStatusReg;
    }

    // store Status register, so it can be restored at the next frame
    // if there are no changes (sprite collision bit is lost)
    tms.oldStatusReg = tms.StatusReg;
    tms.Change = 0;
	return;
}

*/


/*
int TMS9928A_interrupt () {
    int b;

    // when skipping frames, calculate sprite collision
    if (osd_skip_this_frame() ) {
        if (tms.Change) {
            if (TMS_SPRITES_ENABLED) {
                _TMS9928A_sprites (NULL);
            }
        } else {
	    	tms.StatusReg = tms.oldStatusReg;
		}
    }

    tms.StatusReg |= 0x80;
    b = (tms.Regs[1] & 0x20) != 0;
    if (b != tms.INT) {
        tms.INT = b;
        if (tms.INTCallback) tms.INTCallback (tms.INT);
    }

    return b;
}
*/


/** plot_pixel() ********************************************/
/* draws a pixel in osd_bitmap on correct location          */
/* using the correct color                                  */
/************************************************************/
void MSXTMS9928a::plot_pixel(struct osd_bitmap *bitmap,int x,int y,int pen)
{
	bitmap->line[y][x]=pen;
}


/** PutImage() ***********************************************/
/** Put an image on the screen.                             **/
/*************************************************************/
void MSXTMS9928a::PutImage(void)
{

  if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen)<0) return;//ExitNow=1;

  /* Copy image */
  //memcpy(screen->pixels,bitmapscreen->_private,WIDTH*HEIGHT*sizeof(pixel));
  memcpy(screen->pixels,bitmapscreen->_private,WIDTH*HEIGHT*1);

//  /* Handle on-screen message */
//  if (Message != NULL)
//  {
//	PutMessage();
//
//	if (--MessageTimer < 0)
//	{
//	    free(Message);
//	    Message = NULL;
//	    MessageTimer = 0;
//	}
//  }
//
  if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
 
  /* Update screen */
  SDL_UpdateRect(screen,0,0,0,0);

  /* Set timer frequency */
//  SyncFreq=PALVideo? 50:60;
}


/* code taken over from / inspired by mame/mess */
/* Create a bitmap. */
/* VERY IMPORTANT: the function must allocate also a "safety area" 16 pixels wide all */
/* around the bitmap. This is required because, for performance reasons, some graphic */
/* routines don't clip at boundaries of the bitmap. */
struct osd_bitmap *MSXTMS9928a::alloc_bitmap(int width,int height,int depth)
{
	struct osd_bitmap *bitmap;

	if (depth != 8 && depth != 15 && depth != 16 && depth != 32)
	{
		printf( "osd_alloc_bitmap() unknown depth %d\n", 
			depth);
		return 0;
	}

	if ((bitmap = (struct osd_bitmap*)malloc(sizeof(struct osd_bitmap))) != 0)
	{
		unsigned char *bitmap_data=0;
		int i,rowlen,rdwidth, bytes_per_pixel;
		int y_rows,safety;
		int bitmap_size;

		bitmap->depth = depth;
		bitmap->width = width;
		bitmap->height = height;

		safety=(int)((HEIGHT-height)/2);

		rdwidth = (WIDTH + 7) & ~7;     /* round width to a quadword */

		bytes_per_pixel=(depth+7)/8;

		rowlen = bytes_per_pixel * (rdwidth + 2 * safety) * sizeof(unsigned char);
		
		y_rows = height + 2 * safety;

		bitmap_size = y_rows * rowlen ;

		if ((bitmap_data = (byte*)malloc(bitmap_size)) == 0)
		{
			free(bitmap);
			return 0;
		}
	
		/* clear ALL bitmap, including SAFETY area, to avoid garbage on right */
		/* side of screen is width is not a multiple of 4 */
		memset(bitmap_data, 0, (height + 2 * safety) * rowlen);

		if ((bitmap->line = (byte**)malloc((height + 2 * safety) * sizeof(unsigned char *))) == 0)
		{
		    free(bitmap_data);
		    free(bitmap);
		    return 0;
		}

		for (i = 0; i < height + 2 * safety; i++)
		{
		    bitmap->line[i] = &bitmap_data[i * rowlen + safety * bytes_per_pixel];
		}

		bitmap->line += safety;

		bitmap->_private = bitmap_data ;
	}

	return bitmap;
}
