// $Id$
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
** 
** 04/10/2001
** Did correct own version of osd_bitmap and draw border when changing reg 7
** 
** 08/10/2001
** Started converting the all in one approach as a atrter
** Did correct blanking, mode0 renderer and some other stuff
**
** 09/10/2001
** Got all modes working + sprites
** 
** 11/10/2001
** Enable 'dirty' methodes of Sean again + own extra dirty flag
** as a side effect this means that the sprites aren't cleared for the moment :-)
** 
** 12/10/2001
** Miss-used the dirtyNames table to indicate which chars are overdrawn with sprites.
** Noticed some timing issues that need to be looked into.
** 
*/

#include <string>

//#include "driver.h"
//#include "vidhrdw/generic.h"
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXTMS9928a.hh"
#include <cassert>

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


#define IMAGE_SIZE (256*192)        /* size of rendered image        */

#define TMS_SPRITES_ENABLED ((tms.Regs[1] & 0x50) == 0x40)
#define TMS99x8A 1
#define TMS_MODE ( (tms.model == TMS99x8A ? (tms.Regs[0] & 2) : 0) | \
	((tms.Regs[1] & 0x10)>>4) | ((tms.Regs[1] & 8)>>1))

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
**static void _TMS9928A_sprites (struct osd_bitmap*);
**static void _TMS9928A_change_register (int, UINT8);
**
*/

// RENDERERS for one-pass rendering
void MSXTMS9928a::mode0(struct osd_bitmap* bmp){

  int pattern,x,y,yy,xx,name,charcode,colour;
  byte fg,bg,*patternptr;

  name = 0;
  for (y=0;y<24;y++) {
    for (x=0;x<32;x++,name++) {
      charcode = tms.vMem[tms.nametbl+name];
      
      if ( !(tms.DirtyName[name] || tms.DirtyPattern[charcode] ||
	    tms.DirtyColour[charcode/64]) )
	continue;
      
      patternptr = tms.vMem + tms.pattern + charcode*8;
      colour = tms.vMem[tms.colour+charcode/8];
      fg = XPal[colour / 16];
      bg = XPal[colour & 15];
      if (bg == 0) bg = XPal[tms.Regs[7] & 15]; // If transparant then background color

      for (yy=0;yy<8;yy++) {
	pattern=*patternptr++;
	for (xx=0;xx<8;xx++) {
	  plot_pixel (bmp, x*8+xx, y*8+yy,
	      (pattern & 0x80) ? fg : bg);
	      pattern *= 2;
	}
      }
    }
  }
  _TMS9928A_set_dirty (0);
};

void MSXTMS9928a::mode1(struct osd_bitmap* bmp){
  int pattern,x,y,yy,xx,name,charcode;
  byte fg,bg,*patternptr;

  // Not needed since full screen refresh not executed now
  //if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
  //  return;

  fg = XPal[tms.Regs[7] / 16];
  bg = XPal[tms.Regs[7] & 15];

  /* colours at sides must be reset */
  if (tms.anyDirtyColour) {
    for (y=0;y<192;y++){
      for (x=0;x<8;x++){
	plot_pixel (bmp, x, y,bg);
	plot_pixel (bmp, 255-x, y,bg);
      }
    }
  }


  name = 0;
  for (y=0;y<24;y++) {
    for (x=0;x<40;x++,name++) {
      charcode = tms.vMem[tms.nametbl+name];
      if ( !(tms.DirtyName[name] || tms.DirtyPattern[charcode]) &&
	  !tms.anyDirtyColour)
	continue;
      
      patternptr = tms.vMem + tms.pattern + (charcode*8);
      for (yy=0;yy<8;yy++) {
	pattern = *patternptr++;
	for (xx=0;xx<6;xx++) {
	  plot_pixel (bmp, 8+x*6+xx, y*8+yy,
	      (pattern & 0x80) ? fg : bg);
	  pattern *= 2;
	}
      }
    }
  }
  _TMS9928A_set_dirty (0);
};

void MSXTMS9928a::mode2(struct osd_bitmap* bmp){
  int colour,name,x,y,yy,pattern,xx,charcode;
  byte fg,bg;
  byte *colourptr,*patternptr;

  // Not needed since full screen refresh not executed now
  //if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
  //  return;

  name = 0;
  for (y=0;y<24;y++) {
    for (x=0;x<32;x++,name++) {
      charcode = tms.vMem[tms.nametbl+name]+(y/8)*256;
      colour = (charcode&tms.colourmask);
      pattern = (charcode&tms.patternmask);
      if ( !(tms.DirtyName[name] || tms.DirtyPattern[pattern] ||
	    tms.DirtyColour[colour]) )
	continue;
      
      patternptr = tms.vMem+tms.pattern+colour*8;
      colourptr = tms.vMem+tms.colour+pattern*8;
      for (yy=0;yy<8;yy++) {
	pattern = *patternptr++;
	colour = *colourptr++;
	fg = XPal[colour / 16];
	bg = XPal[colour & 15];
	if (bg == 0) bg = XPal[tms.Regs[7] & 15]; // If transparant then background color

	for (xx=0;xx<8;xx++) {
	  plot_pixel (bmp, x*8+xx, y*8+yy,
	      (pattern & 0x80) ? fg : bg);
	  pattern *= 2;
	}
      }
    }
  }
  _TMS9928A_set_dirty (0);
};

void MSXTMS9928a::mode12(struct osd_bitmap* bmp){
  int pattern,x,y,yy,xx,name,charcode;
  byte fg,bg,*patternptr;

  // Not needed since full screen refresh not executed now
  //if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
  //  return;

  fg = XPal[tms.Regs[7] / 16];
  bg = XPal[tms.Regs[7] & 15];

  /* colours at sides must be reset */
  if (tms.anyDirtyColour) {
    for (y=0;y<192;y++){
      for (x=0;x<8;x++){
	plot_pixel (bmp, x, y,bg);
	plot_pixel (bmp, 255-x, y,bg);
      }
    }
  }

  name = 0;
  for (y=0;y<24;y++) {
    for (x=0;x<40;x++,name++) {
      charcode = (tms.vMem[tms.nametbl+name]+(y/8)*256)&tms.patternmask;
      if ( !(tms.DirtyName[name] || tms.DirtyPattern[charcode]) &&
	  !tms.anyDirtyColour)
	continue;
      patternptr = tms.vMem + tms.pattern + (charcode*8);
      for (yy=0;yy<8;yy++) {
	pattern = *patternptr++;
	for (xx=0;xx<6;xx++) {
	  plot_pixel (bmp, 8+x*6+xx, y*8+yy,
	      (pattern & 0x80) ? fg : bg);
	  pattern *= 2;
	}
      }
    }
  }
  _TMS9928A_set_dirty (0);
};

void MSXTMS9928a::mode3(struct osd_bitmap* bmp){
  int x,y,yy,yyy,name,charcode;
  byte fg,bg,*patternptr;

  // Not needed since full screen refresh not executed now
  //if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
  //  return;

  name = 0;
  for (y=0;y<24;y++) {
    for (x=0;x<32;x++,name++) {
      charcode = tms.vMem[tms.nametbl+name];
      if ( !(tms.DirtyName[name] || tms.DirtyPattern[charcode]) &&
	  tms.anyDirtyColour)
	continue;
      patternptr = tms.vMem+tms.pattern+charcode*8+(y&3)*2;
      for (yy=0;yy<2;yy++) {
	fg = XPal[(*patternptr / 16)];
	bg = XPal[((*patternptr++) & 15)];
	if (bg == 0) bg = XPal[tms.Regs[7] & 15]; // If transparant then background color

	for (yyy=0;yyy<4;yyy++) {
	  plot_pixel (bmp, x*8+0, y*8+yy*4+yyy, fg);
	  plot_pixel (bmp, x*8+1, y*8+yy*4+yyy, fg);
	  plot_pixel (bmp, x*8+2, y*8+yy*4+yyy, fg);
	  plot_pixel (bmp, x*8+3, y*8+yy*4+yyy, fg);
	  plot_pixel (bmp, x*8+4, y*8+yy*4+yyy, bg);
	  plot_pixel (bmp, x*8+5, y*8+yy*4+yyy, bg);
	  plot_pixel (bmp, x*8+6, y*8+yy*4+yyy, bg);
	  plot_pixel (bmp, x*8+7, y*8+yy*4+yyy, bg);
	}
      }
    }
  }
  _TMS9928A_set_dirty (0);
};

void MSXTMS9928a::modebogus(struct osd_bitmap* bmp){
  byte fg,bg;
  int x,y,n,xx;

  // Not needed since full screen refresh not executed now
  //if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
  //  return;

  fg = XPal[tms.Regs[7] / 16];
  bg = XPal[tms.Regs[7] & 15];

  for (y=0;y<192;y++) {
    xx=0;
    n=8; while (n--) plot_pixel (bmp, xx++, y, bg);
    for (x=0;x<40;x++) {
      n=4; while (n--) plot_pixel (bmp, xx++, y, fg);
      n=2; while (n--) plot_pixel (bmp, xx++, y, bg);
    }
    n=8; while (n--) plot_pixel (bmp, xx++, y, bg);
  }

  _TMS9928A_set_dirty (0);
};

void MSXTMS9928a::mode23(struct osd_bitmap* bmp){
  int x,y,yy,yyy,name,charcode;
  byte fg,bg,*patternptr;

  // Not needed since full screen refresh not executed now
  //if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
  //  return;

  name = 0;
  for (y=0;y<24;y++) {
    for (x=0;x<32;x++,name++) {
      charcode = tms.vMem[tms.nametbl+name];
      if ( !(tms.DirtyName[name] || tms.DirtyPattern[charcode]) &&
	  tms.anyDirtyColour)
	continue;
      patternptr = tms.vMem + tms.pattern +
	((charcode+(y&3)*2+(y/8)*256)&tms.patternmask)*8;
      for (yy=0;yy<2;yy++) {
	fg = XPal[(*patternptr / 16)];
	bg = XPal[((*patternptr++) & 15)];
	if (bg == 0) bg = XPal[tms.Regs[7] & 15]; // If transparant then background color

	for (yyy=0;yyy<4;yyy++) {
	  plot_pixel (bmp, x*8+0, y*8+yy*4+yyy, fg);
	  plot_pixel (bmp, x*8+1, y*8+yy*4+yyy, fg);
	  plot_pixel (bmp, x*8+2, y*8+yy*4+yyy, fg);
	  plot_pixel (bmp, x*8+3, y*8+yy*4+yyy, fg);
	  plot_pixel (bmp, x*8+4, y*8+yy*4+yyy, bg);
	  plot_pixel (bmp, x*8+5, y*8+yy*4+yyy, bg);
	  plot_pixel (bmp, x*8+6, y*8+yy*4+yyy, bg);
	  plot_pixel (bmp, x*8+7, y*8+yy*4+yyy, bg);
	}
      }
    }
  }
  _TMS9928A_set_dirty (0);
};

/*
** This function renders the sprites. Sprite collision is calculated in
** in a back buffer (tms.dBackMem), because sprite collision detection
** is rather complicated (transparent sprites also cause the sprite
** collision bit to be set, and ``illegal'' sprites do not count
** (they're not displayed)).
**
** This code should be optimized. One day.
**
*/
void MSXTMS9928a::sprites(struct osd_bitmap* bmp){
    byte *attributeptr,*patternptr,c;
    int p,x,y,size,i,j,large,yy,xx,limit[192],
        illegalsprite,illegalspriteline;
    word line,line2;
    int dirtycheat;

    attributeptr = tms.vMem + tms.spriteattribute;
    size = (tms.Regs[1] & 2) ? 16 : 8;
    large = (int)(tms.Regs[1] & 1);

    for (x=0;x<192;x++) limit[x] = 4;
    tms.StatusReg = 0x80;
    illegalspriteline = 255;
    illegalsprite = 0;

    memset (tms.dBackMem, 0, IMAGE_SIZE);
    for (p=0;p<32;p++) {
      y = *attributeptr++;
        if (y == 208) break;
        if (y > 208) {
            y=-(~y&255);
        } else {
            y++;
        }
        x = *attributeptr++;
        patternptr = tms.vMem + tms.spritepattern +
            ((size == 16) ? *attributeptr & 0xfc : *attributeptr) * 8;
        attributeptr++;
        c = (*attributeptr & 0x0f);
        if (*attributeptr & 0x80) x -= 32;
        attributeptr++;

	// Hack: mark the characters that are toughed by the sprites as dirty
	// This way we also redraw the affected chars 
	// sprites are only visible in screen modes of 32 chars wide
	
	dirtycheat=(x>>3)+(y>>3)*32;
	//mark four chars dirty (2x2)
	tms.DirtyName[dirtycheat]=1;
	tms.DirtyName[dirtycheat+1]=1;
	tms.DirtyName[dirtycheat+32]=1;
	tms.DirtyName[dirtycheat+33]=1;
	//if needed extra 5 around them also (3x3)
	if ((size == 16) || large){
	tms.DirtyName[dirtycheat+2]=1;
	tms.DirtyName[dirtycheat+34]=1;
	tms.DirtyName[dirtycheat+64]=1;
	tms.DirtyName[dirtycheat+65]=1;
	tms.DirtyName[dirtycheat+66]=1;
	}
	//if needed extra 7 around them also (4x4)
	if ((size == 16) && large){
	tms.DirtyName[dirtycheat+3]=1;
	tms.DirtyName[dirtycheat+35]=1;
	tms.DirtyName[dirtycheat+65]=1;
	tms.DirtyName[dirtycheat+96]=1;
	tms.DirtyName[dirtycheat+97]=1;
	tms.DirtyName[dirtycheat+98]=1;
	tms.DirtyName[dirtycheat+99]=1;
	}
	// worst case is 32*24+99(=867)
	// DirtyName 40x24(=960)  => is large enough 
	tms.anyDirtyName =1;
	// No need to clean sprites if no vram write so tms.Change could remain false;

        if (!large) {
            /* draw sprite (not enlarged) */
            for (yy=y;yy<(y+size);yy++) {
                if ( (yy < 0) || (yy > 191) ) continue;
                if (limit[yy] == 0) {
                    /* illegal sprite line */
                    if (yy < illegalspriteline) {
                        illegalspriteline = yy;
                        illegalsprite = p;
                    } else if (illegalspriteline == yy) {
                        if (illegalsprite > p) {
                            illegalsprite = p;
                        }
                    }
                    if (tms.LimitSprites) continue;
                } else limit[yy]--;
                line = 256*patternptr[yy-y] + patternptr[yy-y+16];
                for (xx=x;xx<(x+size);xx++) {
                    if (line & 0x8000) {
                        if ((xx >= 0) && (xx < 256)) {
                            if (tms.dBackMem[yy*256+xx]) {
                                tms.StatusReg |= 0x20;
                            } else {
                                tms.dBackMem[yy*256+xx] = 0x01;
                            }
                            if (c && ! (tms.dBackMem[yy*256+xx] & 0x02))
                            {
                            	tms.dBackMem[yy*256+xx] |= 0x02;
                            	if (bmp)
									plot_pixel (bmp, xx, yy, XPal[c]);
							}
                        }
                    }
                    line *= 2;
                }
            }
        } else {
            /* draw enlarged sprite */
            for (i=0;i<size;i++) {
                yy=y+i*2;
                line2 = 256*patternptr[i] + patternptr[i+16];
                for (j=0;j<2;j++) {
                    if ( (yy >= 0) && (yy <= 191) ) {
                        if (limit[yy] == 0) {
                            /* illegal sprite line */
                            if (yy < illegalspriteline) {
                                illegalspriteline = yy;
                                 illegalsprite = p;
                            } else if (illegalspriteline == yy) {
                                if (illegalsprite > p) {
                                    illegalsprite = p;
                                }
                            }
                            if (tms.LimitSprites) continue;
                        } else limit[yy]--;
                        line = line2;
                        for (xx=x;xx<(x+size*2);xx+=2) {
                            if (line & 0x8000) {
                                if ((xx >=0) && (xx < 256)) {
                                    if (tms.dBackMem[yy*256+xx]) {
                                        tms.StatusReg |= 0x20;
                                    } else {
                                        tms.dBackMem[yy*256+xx] = 0x01;
                                    }
		                            if (c && ! (tms.dBackMem[yy*256+xx] & 0x02))
        		                    {
                		            	tms.dBackMem[yy*256+xx] |= 0x02;
                                        if (bmp)
                                        	plot_pixel (bmp, xx, yy, XPal[c]);
                		            }
                                }
                                if (((xx+1) >=0) && ((xx+1) < 256)) {
                                    if (tms.dBackMem[yy*256+xx+1]) {
                                        tms.StatusReg |= 0x20;
                                    } else {
                                        tms.dBackMem[yy*256+xx+1] = 0x01;
                                    }
		                            if (c && ! (tms.dBackMem[yy*256+xx+1] & 0x02))
        		                    {
                		            	tms.dBackMem[yy*256+xx+1] |= 0x02;
                                        if (bmp)
                                        	plot_pixel (bmp, xx+1, yy, XPal[c]);
									}
                                }
                            }
                            line *= 2;
                        }
                    }
                    yy++;
                }
            }
        }
    }
    if (illegalspriteline == 255) {
        tms.StatusReg |= (p > 31) ? 31 : p;
    } else {
        tms.StatusReg |= 0x40 + illegalsprite;
    }
}

/*
void (*ModeHandlers[])(struct osd_bitmap*) = {
        &MSXTMS9928a::mode0, &MSXTMS9928a::mode1, &MSXTMS9928a::mode2,  &MSXTMS9928a::mode12,
        &MSXTMS9928a::mode3, &MSXTMS9928a::modebogus, &MSXTMS9928a::mode23,
        &MSXTMS9928a::modebogus };
*/

/*
typedef void (MSXTMS9928a::*moderoutines)(struct osd_bitmap*);
moderoutines ModeHandlers[] = {
        &MSXTMS9928a::mode0, &MSXTMS9928a::mode1, &MSXTMS9928a::mode2,  &MSXTMS9928a::mode12,
        &MSXTMS9928a::mode3, &MSXTMS9928a::modebogus, &MSXTMS9928a::mode23,
        &MSXTMS9928a::modebogus };
*/

/*void (*ModeHandlers[])(struct osd_bitmap*) = {
        mode0, mode1, mode2,  mode12,
        mode3, modebogus, mode23,
        modebogus };
*/

/*
** initialize the palette 
*/
//void tms9928A_init_palette (unsigned char *palette, 
//	unsigned short *colortable,const unsigned char *color_prom) {
//    memcpy (palette, &TMS9928A_palette, sizeof(TMS9928A_palette));
//}

MSXTMS9928a::MSXTMS9928a() : currentTime(3579545, 0)
{
	PRT_DEBUG ("Creating an MSXTMS9928a object");
};
MSXTMS9928a::~MSXTMS9928a()
{
	PRT_DEBUG ("Destroying an MSXTMS9928a object");
};

/*
** The init, start, reset and shutdown functions
*/
void MSXTMS9928a::reset ()
{
	MSXDevice::reset();

	for (int i=0;i<8;i++) tms.Regs[i] = 0;
	tms.StatusReg = 0;
	tms.nametbl = tms.pattern = tms.colour = 0;
	tms.spritepattern = tms.spriteattribute = 0;
	tms.colourmask = tms.patternmask = 0;
	tms.Addr = tms.ReadAhead = 0;
    //tms.INT = 0;
	tms.mode = tms.BackColour = 0;
	tms.Change = 1;
	tms.FirstByte = -1;
	_TMS9928A_set_dirty (1);
};

//int TMS9928A_start (int model, unsigned int vram) 
void MSXTMS9928a::init(void)
{
	MSXDevice::init();
    // /* 4 or 16 kB vram please */
    // if (! ((vram == 0x1000) || (vram == 0x4000) || (vram == 0x2000)) )
    //    return 1;

    //tms.model = model;
    tms.model = 1;	//tms9928a

    bitmapscreen= alloc_bitmap (256,192 , 8);
    if (!bitmapscreen) return ;//(1);

    /* Video RAM */
    tms.vramsize = 0x4000;
    tms.vMem = new byte [tms.vramsize];
    if (!tms.vMem) return ;//(1);
    memset (tms.vMem, 0, tms.vramsize);

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
    PRT_DEBUG ("OK\n  Opening display...");
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

    return ;//0;
};

void MSXTMS9928a::start()
{
	MSXDevice::start();
	//First interrupt in Pal mode here
	Scheduler::instance()->setSyncPoint(currentTime+71285, *this);
	PutImage();
};

void MSXTMS9928a::executeUntilEmuTime(const Emutime &time)
{
	PRT_DEBUG("Executing TMS9928a at time ");

	//TODO:: Change from full screen refresh to emutime based!!
	if (tms.stateChanged){
	  fullScreenRefresh();
	  PutImage();
	  tms.stateChanged=false;
	  };
	
	//Next SP/interrupt in Pal mode here
	currentTime = time;
	Scheduler::instance()->setSyncPoint(currentTime+85542, *this); //71285 for NTS, 85542 for PAL
    // Since this is the vertical refresh
    tms.StatusReg |= 0x80;
	// Set interrupt if bits enable it
    if (tms.Regs[1] & 0x20) {
        setInterrupt();
    }
};

/*
~MSXTMS9928a()
{
    free (tms.vMem); tms.vMem = NULL;
    free (tms.dBackMem); tms.dBackMem = NULL;
    free (tms.DirtyColour); tms.DirtyColour = NULL;
    free (tms.DirtyName); tms.DirtyName = NULL;
    free (tms.DirtyPattern); tms.DirtyPattern = NULL;
    osd_free_bitmap (tms.tmpbmp); tms.tmpbmp = NULL;
};
*/

/*
** Set all dirty / clean
*
*/
void MSXTMS9928a::_TMS9928A_set_dirty (char dirty) {
    tms.anyDirtyColour = tms.anyDirtyName = tms.anyDirtyPattern = dirty;
    memset (tms.DirtyName, dirty, MAX_DIRTY_NAME);
    memset (tms.DirtyColour, dirty, MAX_DIRTY_COLOUR);
    memset (tms.DirtyPattern, dirty, MAX_DIRTY_PATTERN);
};

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
	tms.stateChanged=true;

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
	  tms.stateChanged=true;
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
    default:
      assert(false);
  }
};

byte MSXTMS9928a::TMS9928A_vram_r()
{
      //READ_HANDLER (TMS9928A_vram_r) 
      byte b;
      b = tms.ReadAhead;
      tms.ReadAhead = tms.vMem[tms.Addr];
      tms.Addr = (tms.Addr + 1) & (tms.vramsize - 1);
      tms.FirstByte = -1;
      return b;
};

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
		tms.FirstByte = -1;
        resetInterrupt();
		return b;
	default:
		assert(false);
	}
};


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

    PRT_DEBUG("TMS9928A: Reg " << (int)reg << " = " << (int)val);
            PRT_DEBUG ("TMS9928A: now in mode " << tms.mode );
            // Next lines causes crashes
	    //PRT_DEBUG (sprintf("TMS9928A: %s\n", modes[tms.mode]));
    tms.Change = 1;
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
            //PRT_DEBUG ("TMS9928A: now in mode " << tms.mode );
            //PRT_DEBUG (sprintf("TMS9928A: %s\n", modes[tms.mode]));
            _TMS9928A_set_dirty (1);
        }
        break;
    case 1:
        // check for changes in the INT line
        if ( (val & 0x20) && (tms.StatusReg & 0x80) ){
          /* Set the interrupt line !! */
          setInterrupt();
        };
        mode = TMS_MODE;
        if (tms.mode != mode) {
            tms.mode = mode;
            _TMS9928A_set_dirty (1);
            printf("TMS9928A: %s\n", modes[tms.mode]);
            //PRT_DEBUG (sprintf("TMS9928A: %s\n", modes[tms.mode]));
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
        //currently I change the entire background
        // this should be on a line base 
        full_border_fil();

        break;
    }
};


void MSXTMS9928a::fullScreenRefresh()
{
    int c,i,j;

    // Change background color from value if needed
    //
    /* Changed: if color is drawn in color 0 then get correct color from register 7 
    // instead of fidling with the entire color table. This was a
    // remaining thing from Sean all-in-one screen build method
    if (tms.Change) {
        c = tms.Regs[7] & 15; if (!c) c=1;
        if (tms.BackColour != c) {
            tms.BackColour = c;
	    XPal[0]=SDL_MapRGB(screen->format,
		    TMS9928A_palette[c*3],
		    TMS9928A_palette[c*3 + 1],
		    TMS9928A_palette[c*3 + 2]);
	    //full_border_fil(); // already done in changing R7 itself
        }
    }
    */
    
    // Really redraw if needed
    if (tms.Change) {
        if (! (tms.Regs[1] & 0x40) ) {
            // screen blanked so all background color
	    c= XPal[tms.Regs[7] & 15];
	    for (i=0;i<256;i++){
	      for (j=0;j<192;j++){
	        plot_pixel(bitmapscreen,i,j,c);
	      }
	    }
        } else {
	  // draw background
          
	  //ModeHandlers[tms.mode](bitmapscreen);
	  switch (tms.mode){
	    case 0: mode0(bitmapscreen);
		    break;
	    case 1: mode1(bitmapscreen);
		    break;
	    case 2: mode2(bitmapscreen);
		    break;
	    case 3: mode12(bitmapscreen);
		    break;
	    case 4: mode3(bitmapscreen);
		    break;
	    case 5: modebogus(bitmapscreen);
		    break;
	    case 6: mode23(bitmapscreen);
		    break;
	    case 7: modebogus(bitmapscreen);
	  }
	  
	  // if sprites enabled in this mode
	  if (TMS_SPRITES_ENABLED ) {
	    //PRT_DEBUG("TMS9928A: Need to include sprite routines.");
	    // We just render them over the current image
	    sprites(bitmapscreen);
	  }
        }
    } 
    /*
    else {
		tms.StatusReg = tms.oldStatusReg;
    }
    */

}

void MSXTMS9928a::full_border_fil()
{
     int i,j;
     int bytes_per_pixel,rowlen,offset;

     bytes_per_pixel=(bitmapscreen->depth+7)/8;
     rowlen = bytes_per_pixel * (bitmapscreen->width + 2 * bitmapscreen->safetx);
     offset = (bitmapscreen->safety + bitmapscreen->height)*rowlen;

     for (i=0;i<(bitmapscreen->safety)*rowlen;i++){
       bitmapscreen->_private[i] = XPal[tms.Regs[7] & 0xf] ;
       bitmapscreen->_private[offset + i] = XPal[tms.Regs[7] & 0xf] ;
     }
     for (i = bitmapscreen->safety * rowlen - bitmapscreen->safetx;
	 i < (1 + bitmapscreen->height + bitmapscreen->safety )*rowlen;
	 i+=rowlen){
       for (j=0;j<bitmapscreen->safetx*2;j++){
	 bitmapscreen->_private[j+i] = XPal[tms.Regs[7] & 0xf] ;
       }
     }
}


/*
** Interface functions
*/


//void TMS9928A_set_spriteslimit (int limit) {
//    tms.LimitSprites = limit;
//};

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
};

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
};
*/


/** plot_pixel() ********************************************/
/* draws a pixel in osd_bitmap on correct location          */
/* using the correct color                                  */
/************************************************************/
void MSXTMS9928a::plot_pixel(struct osd_bitmap *bitmap,int x,int y,int pen)
{
	bitmap->line[y][x]=pen;
};


/** PutImage() ***********************************************/
/** Put an image on the screen.                             **/
/*************************************************************/
int MSXTMS9928a::debugColor = 2;	// debug

void MSXTMS9928a::PutImage(void)
{

  if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen)<0) return;//ExitNow=1;

  //debug code
  /*
	for (byte i=0; i<190 ;i++) {
		for (byte j=0;j<30;j++){
		  plot_pixel(bitmapscreen ,i+j,i,XPal[debugColor]);
		  plot_pixel(bitmapscreen ,255-i-j,i,XPal[debugColor]);
		}
		debugColor = (debugColor++)&0x0f;
		}
   */

  /* Copy image */
  //memcpy(screen->pixels,bitmapscreen->_private,WIDTH*HEIGHT*sizeof(pixel));
  memcpy(screen->pixels,bitmapscreen->_private,WIDTH*HEIGHT*1);

  if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
 
  /* Update screen */
  SDL_UpdateRect(screen,0,0,0,0);

  /* Set timer frequency */
//  SyncFreq=PALVideo? 50:60;
};


/* code taken over from / inspired by mame/mess */
/* Create a bitmap. */
struct osd_bitmap *MSXTMS9928a::alloc_bitmap(int width,int height,int depth)
{
	struct osd_bitmap *bitmap;

	if (depth != 8 && depth != 15 && depth != 16 && depth != 32)
	{
		printf( "osd_alloc_bitmap() unknown depth %d\n", 
			depth);
		return 0;
	}

	if ((bitmap = new struct osd_bitmap) != 0)
	{
		unsigned char *bitmap_data=0;
		int i,rowlen,bytes_per_pixel;
		int y_rows;
		int bitmap_size;

		bitmap->depth = depth;
		bitmap->width = width;
		bitmap->height = height;
		
		// does the border otherwise mismatch between
		// size of the SDL buffer and the 
		// (osd_)bitmap_data
		bitmap->safetx=(WIDTH-width)/2;
		bitmap->safety=(HEIGHT-height)/2;
		bytes_per_pixel=(depth+7)/8;

		rowlen = bytes_per_pixel * (width + 2 * bitmap->safetx);
		y_rows = height + 2 * bitmap->safety;

		bitmap_size = y_rows * rowlen ;

		if ((bitmap_data = new byte[bitmap_size]) == 0) {
			delete bitmap;
			return 0;
		}
	
		/* clear ALL bitmap, including SAFETY area, to avoid garbage on right */
		/* side of screen is width is not a multiple of 4 */
		memset(bitmap_data, 0, bitmap_size);

		if ((bitmap->line = new byte*[height + 2 * bitmap->safety ]) == 0)
		{
		    delete[] bitmap_data;
		    delete bitmap;
		    return 0;
		}

		for (i = 0; i < height ; i++)
		{
		    bitmap->line[i] = bitmap_data + bitmap->safetx + (i + bitmap->safety) * rowlen ;
		}

		bitmap->_private = bitmap_data ;
	}

	return bitmap;
};
