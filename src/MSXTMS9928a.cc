// $Id$

/* TODO:
- Verify the dirty checks, especially those of mode3 and mode23,
  which were different before.
- Draw sprites per line.

Idea:
For bitmap modes, cache VRAM lines rather than screen lines.
Better performance when doing raster tricks or double buffering.
Probably also easier to implement when using line buffers.
*/

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

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXTMS9928a.hh"

#include <string>
#include <cassert>


#define TMS_SPRITES_ENABLED ((tms.Regs[1] & 0x50) == 0x40)
#define TMS99x8A 1
#define TMS_MODE ( (tms.model == TMS99x8A ? (tms.Regs[0] & 2) : 0) | \
	((tms.Regs[1] & 0x10)>>4) | ((tms.Regs[1] & 8)>>1))

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
static unsigned char TMS9928A_palette[16 * 3] =
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

// Defines for `dirty' optimization
#define MAX_DIRTY_COLOUR        (256*3)
#define MAX_DIRTY_PATTERN       (256*3)
#define MAX_DIRTY_NAME          (40*24)

// RENDERERS for line-based scanline conversion

void MSXTMS9928a::mode0(Pixel *pixelPtr, int line)
{
	Pixel backDropColour = XPal[tms.Regs[7] & 0x0F];
	int name = (line / 8) * 32;
	// TODO: charLefts may have slightly better performance,
	// but x may be easier to convert to pixel-based rendering.
	for (int charsLeft = 32; charsLeft--; ) {
		int charcode = tms.vMem[tms.nametbl + name];
		// TODO: is tms.DirtyColour[charcode / 64] correct?
		if (tms.DirtyName[name] || tms.DirtyPattern[charcode]
		|| tms.DirtyColour[charcode / 64]) {

			int colour = tms.vMem[tms.colour + charcode / 8];
			Pixel fg = colour >> 4;   fg = (fg ? XPal[fg] : backDropColour);
			Pixel bg = colour & 0x0F; bg = (bg ? XPal[bg] : backDropColour);

			int pattern = tms.vMem[tms.pattern + charcode * 8 + (line & 0x07)];
			// TODO: Compare performance of this loop vs unrolling.
			for (int i = 8; i--; ) {
				*pixelPtr++ = ((pattern & 0x80) ? fg : bg);
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 8;
		}
		name++;
	}
}

void MSXTMS9928a::mode1(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
	//  return;
	if (!tms.anyDirtyColour) return;

	Pixel fg = XPal[tms.Regs[7] >> 4];
	Pixel bg = XPal[tms.Regs[7] & 0x0F];

	int name = (line / 8) * 40;
	int x = 0;
	// Extended left border.
	for (; x < 8; x++) *pixelPtr++ = bg;
	// Actual display.
	while (x < 248) {
		int charcode = tms.vMem[tms.nametbl + name];
		if (tms.DirtyName[name] || tms.DirtyPattern[charcode]) {
			int pattern = tms.vMem[tms.pattern + charcode * 8 + (line & 7)];
			for (int i = 6; i--; ) {
				*pixelPtr++ = ((pattern & 0x80) ? fg : bg);
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 6;
		}
		x += 6;
		name++;
	}
	// Extended right border.
	for (; x < 256; x++) *pixelPtr++ = bg;
}

void MSXTMS9928a::mode2(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
	//  return;

	Pixel backDropColour = XPal[tms.Regs[7] & 0x0F];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = tms.vMem[tms.nametbl+name] + (line / 64) * 256;
		int colourNr = (charcode & tms.colourmask);
		int patternNr = (charcode & tms.patternmask);
		if (tms.DirtyName[name] || tms.DirtyPattern[patternNr]
		|| tms.DirtyColour[colourNr]) {
			// TODO: pattern uses colourNr and colour uses patterNr...
			//      I don't get it.
			int pattern = tms.vMem[tms.pattern + colourNr * 8 + (line & 7)];
			int colour = tms.vMem[tms.colour + patternNr * 8 + (line & 7)];
			Pixel fg = colour >> 4;   fg = (fg ? XPal[fg] : backDropColour);
			Pixel bg = colour & 0x0F; bg = (bg ? XPal[bg] : backDropColour);
			for (int i = 8; i--; ) {
				*pixelPtr++ = ((pattern & 0x80) ? fg : bg);
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 8;
		}
		name++;
	}
}

void MSXTMS9928a::mode12(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
	//  return;
	if (!tms.anyDirtyColour) return;

	Pixel fg = XPal[tms.Regs[7] >> 4];
	Pixel bg = XPal[tms.Regs[7] & 0x0F];

	int name = (line / 8) * 32;
	int x = 0;
	// Extended left border.
	for ( ; x < 8; x++) *pixelPtr++ = bg;
	// Actual display.
	while (x < 248) {
		int charcode = (tms.vMem[tms.nametbl + name] + (line / 64) * 256) & tms.patternmask;
		if (tms.DirtyName[name] || tms.DirtyPattern[charcode]) {
			int pattern = tms.vMem[tms.pattern + charcode * 8];
			for (int i = 6; i--; ) {
				*pixelPtr++ = ((pattern & 0x80) ? fg : bg);
				pattern <<= 1;
			}
		}
		else {
			pixelPtr += 6;
		}
		x += 6;
		name++;
	}
	// Extended right border.
	for ( ; x < 256; x++) *pixelPtr++ = bg;
}

void MSXTMS9928a::mode3(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
	//  return;
	if (!tms.anyDirtyColour) return;

	Pixel backDropColour = XPal[tms.Regs[7] & 0x0F];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = tms.vMem[tms.nametbl + name];
		if (tms.DirtyName[name] || tms.DirtyPattern[charcode]) {
			int colour = tms.vMem[tms.pattern + charcode * 8 + ((line / 4) & 7)];
			Pixel fg = colour >> 4;   fg = (fg ? XPal[fg] : backDropColour);
			Pixel bg = colour & 0x0F; bg = (bg ? XPal[bg] : backDropColour);
			int n;
			for (n = 4; n--; ) *pixelPtr++ = fg;
			for (n = 4; n--; ) *pixelPtr++ = bg;
		}
		else {
			pixelPtr += 8;
		}
		name++;
	}
}

void MSXTMS9928a::modebogus(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
	//  return;

	Pixel fg = XPal[tms.Regs[7] >> 4];
	Pixel bg = XPal[tms.Regs[7] & 0x0F];
	int x = 0;
	for (; x < 8; x++) *pixelPtr++ = bg;
	for (; x < 248; x += 6) {
		int n;
		for (n = 4; n--; ) *pixelPtr++ = fg;
		for (n = 2; n--; ) *pixelPtr++ = bg;
	}
	for (; x < 256; x++) *pixelPtr++ = bg;
}

void MSXTMS9928a::mode23(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(tms.anyDirtyColour || tms.anyDirtyName || tms.anyDirtyPattern) )
	//  return;
	if (!tms.anyDirtyColour) return;

	Pixel backDropColour = XPal[tms.Regs[7] & 0x0F];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = tms.vMem[tms.nametbl + name];
		if (tms.DirtyName[name] || tms.DirtyPattern[charcode]) {
			int colour = tms.vMem[tms.pattern + ((charcode + ((line / 4) & 7) +
				(line / 64) * 256) & tms.patternmask) * 8];
			Pixel fg = colour >> 4;   fg = (fg ? XPal[fg] : backDropColour);
			Pixel bg = colour & 0x0F; bg = (bg ? XPal[bg] : backDropColour);
			int n;
			for (n = 4; n--; ) *pixelPtr++ = fg;
			for (n = 4; n--; ) *pixelPtr++ = bg;
		}
		else {
			pixelPtr += 8;
		}
		name++;
	}
}

/** TODO: Is blanking really a mode?
  */
void MSXTMS9928a::modeblank(Pixel *pixelPtr, int line)
{
	// Screen blanked so all background colour.
	Pixel colour = XPal[tms.Regs[7] & 0x0F];
	for (int x = 0; x < 256; x++) {
		*pixelPtr++ = colour;
	}
}

MSXTMS9928a::RenderMethod MSXTMS9928a::modeToRenderMethod[] = {
	&MSXTMS9928a::mode0,
	&MSXTMS9928a::mode1,
	&MSXTMS9928a::mode2,
	&MSXTMS9928a::mode12,
	&MSXTMS9928a::mode3,
	&MSXTMS9928a::modebogus,
	&MSXTMS9928a::mode23,
	&MSXTMS9928a::modebogus
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
void MSXTMS9928a::sprites(Pixel *linePtrs[], int displayX)
{
	byte *attributeptr = tms.vMem + tms.spriteattribute;
	int size = (tms.Regs[1] & 2) ? 16 : 8;
	int large = (int)(tms.Regs[1] & 1);

	int y, limit[192];
	for (y = 0; y < 192; y++) limit[y] = 4;
	tms.StatusReg = 0x80;
	int illegalspriteline = 255;
	int illegalsprite = 0;

	memset(tms.dBackMem, 0, 256 * 192);
	int p;
	for (p = 0; p < 32; p++) {
		y = *attributeptr++;
		if (y == 208) break;
		y = (y > 208 ? y - 255 : y + 1);
		int x = *attributeptr++;
		byte *patternptr = tms.vMem + tms.spritepattern +
			((size == 16) ? *attributeptr & 0xfc : *attributeptr) * 8;
		attributeptr++;
		Pixel colour = (*attributeptr & 0x0f);
		if (*attributeptr & 0x80) x -= 32;
		attributeptr++;

		// Hack: mark the characters that are touched by the sprites as dirty
		// This way we also redraw the affected chars
		// sprites are only visible in screen modes of 32 chars wide
		int dirtycheat = (x >> 3) + (y >> 3) * 32;
		//mark four chars dirty (2x2)
		tms.DirtyName[dirtycheat] = 1;
		tms.DirtyName[dirtycheat+1] = 1;
		tms.DirtyName[dirtycheat+32] = 1;
		tms.DirtyName[dirtycheat+33] = 1;
		//if needed extra 5 around them also (3x3)
		if ((size == 16) || large){
			tms.DirtyName[dirtycheat+2] = 1;
			tms.DirtyName[dirtycheat+34] = 1;
			tms.DirtyName[dirtycheat+64] = 1;
			tms.DirtyName[dirtycheat+65] = 1;
			tms.DirtyName[dirtycheat+66] = 1;
		}
		//if needed extra 7 around them also (4x4)
		if ((size == 16) && large){
			tms.DirtyName[dirtycheat+3] = 1;
			tms.DirtyName[dirtycheat+35] = 1;
			tms.DirtyName[dirtycheat+65] = 1;
			tms.DirtyName[dirtycheat+96] = 1;
			tms.DirtyName[dirtycheat+97] = 1;
			tms.DirtyName[dirtycheat+98] = 1;
			tms.DirtyName[dirtycheat+99] = 1;
		}
		// worst case is 32*24+99(=867)
		// DirtyName 40x24(=960)  => is large enough
		tms.anyDirtyName = 1;
		// No need to clean sprites if no vram write so tms.Change could remain false;

		if (!large) {
			/* draw sprite (not enlarged) */
			for (int yy = y; yy < (y + size); yy++) {
				if ( (yy < 0) || (yy >= 192) ) continue;
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
				}
				else {
					limit[yy]--;
				}
				word line = (patternptr[yy - y] << 8) | patternptr[yy - y + 16];
				byte *dBackMemPtr = &tms.dBackMem[yy * 256 + x];
				Pixel *pixelPtr = &linePtrs[yy][displayX + x];
				for (int xx = x; xx < (x + size); xx++) {
					if (line & 0x8000) {
						if ((xx >= 0) && (xx < 256)) {
							if (*dBackMemPtr) {
								tms.StatusReg |= 0x20;
							}
							else {
								*dBackMemPtr = 0x01;
							}
							if (colour && !(*dBackMemPtr & 0x02)) {
								*dBackMemPtr |= 0x02;
								*pixelPtr = XPal[colour];
							}
						}
					}
					dBackMemPtr++;
					pixelPtr++;
					line <<= 1;
				}
			}
		}
		else {
			/* draw enlarged sprite */
			for (int i = 0; i < size; i++) {
				int yy = y + i * 2;
				word line2 = (patternptr[i] << 8) | patternptr[i+16];
				for (int j = 0; j < 2; j++) {
					if ((yy >= 0) && (yy < 192)) {
						if (limit[yy] == 0) {
							/* illegal sprite line */
							if (yy < illegalspriteline) {
								illegalspriteline = yy;
								illegalsprite = p;
							}
							else if (illegalspriteline == yy) {
								if (illegalsprite > p) {
									illegalsprite = p;
								}
							}
							if (tms.LimitSprites) continue;
						}
						else {
							limit[yy]--;
						}
						word line = line2;
						byte *dBackMemPtr = &tms.dBackMem[yy * 256 + x];
						Pixel *pixelPtr = &linePtrs[yy][displayX + x];
						for (int xx = x; xx < (x + size * 2); xx++) {
							if (line & 0x8000) {
								if ((xx >= 0) && (xx < 256)) {
									if (*dBackMemPtr) {
										tms.StatusReg |= 0x20;
									}
									else {
										*dBackMemPtr = 0x01;
									}
									if (colour && !(*dBackMemPtr & 0x02)) {
										*dBackMemPtr |= 0x02;
										*pixelPtr = XPal[colour];
									}
								}
							}
							dBackMemPtr++;
							pixelPtr++;
							// Shift on every odd pixel.
							if ((xx - x) & 1) line <<= 1;
						}
					}
					yy++;
				}
			}
		}
	}
	if (illegalspriteline == 255) {
		tms.StatusReg |= (p > 31) ? 31 : p;
	}
	else {
		tms.StatusReg |= 0x40 + illegalsprite;
	}
}

inline static void drawEmptyLine(Pixel *linePtr, Pixel colour)
{
	for (int i = WIDTH; i--; ) {
		*linePtr++ = colour;
	}
}

inline static void drawBorders(
	Pixel *linePtr, Pixel colour, int displayStart, int displayStop)
{
	for (int i = displayStart; i--; ) {
		*linePtr++ = colour;
	}
	linePtr += displayStop - displayStart;
	for (int i = WIDTH - displayStop; i--; ) {
		*linePtr++ = colour;
	}
}

void MSXTMS9928a::fullScreenRefresh()
{
	// Only redraw if needed.
	if (!tms.Change) return;

	Pixel borderColour = XPal[tms.Regs[7] & 0x0F];
	int displayX = (WIDTH - 256) / 2;
	int displayY = (HEIGHT - 192) / 2;
	int line = 0;
	Pixel *currBorderColoursPtr = currBorderColours;

	// Top border.
	for (; line < displayY; line++, currBorderColoursPtr++) {
		if (*currBorderColoursPtr != borderColour) {
			drawEmptyLine(linePtrs[line], borderColour);
			*currBorderColoursPtr = borderColour;
		}
	}

	// Display area.
	RenderMethod renderMethod = modeToRenderMethod[tms.mode];
	for (int y = 0; y < 192; y++, line++, currBorderColoursPtr++) {
		if (tms.Regs[1] & 0x40) {
			// Draw background.
			(this->*renderMethod)(&linePtrs[line][displayX], y);
		}
		else {
			modeblank(&linePtrs[line][displayX], y);
		}
		if (*currBorderColoursPtr != borderColour) {
			drawBorders(linePtrs[line], borderColour,
				displayX, WIDTH - displayX);
			*currBorderColoursPtr = borderColour;
		}
	}

	// Bottom border.
	for (; line < HEIGHT; line++, currBorderColoursPtr++) {
		if (*currBorderColoursPtr != borderColour) {
			drawEmptyLine(linePtrs[line], borderColour);
			*currBorderColoursPtr = borderColour;
		}
	}

	// TODO: Verify interaction between dirty flags and blanking.
	_TMS9928A_set_dirty(0);

	// Draw sprites if enabled in this mode.
	if ((tms.Regs[1] & 0x40) && (TMS_SPRITES_ENABLED)) {
		sprites(&linePtrs[displayY], displayX);
	}
}

void MSXTMS9928a::putImage(void)
{
	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen)<0) return;//ExitNow=1;

	// Copy image.
	memcpy(screen->pixels, pixelData, sizeof(pixelData));

	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

	// Update screen.
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}


MSXTMS9928a::MSXTMS9928a() : currentTime(3579545, 0)
{
	PRT_DEBUG("Creating an MSXTMS9928a object");
}

MSXTMS9928a::~MSXTMS9928a()
{
	PRT_DEBUG("Destroying an MSXTMS9928a object");
}

// The init, start, reset and shutdown functions

void MSXTMS9928a::reset ()
{
	MSXDevice::reset();

	for (int i = 0; i < 8; i++) tms.Regs[i] = 0;
	tms.StatusReg = 0;
	tms.nametbl = tms.pattern = tms.colour = 0;
	tms.spritepattern = tms.spriteattribute = 0;
	tms.colourmask = tms.patternmask = 0;
	tms.Addr = tms.ReadAhead = 0;
	//tms.INT = 0;
	tms.mode = tms.BackColour = 0;
	tms.Change = 1;
	tms.FirstByte = -1;
	_TMS9928A_set_dirty(1);
}

//int TMS9928A_start (int model, unsigned int vram)
void MSXTMS9928a::init(void)
{
	MSXDevice::init();
	// /* 4 or 16 kB vram please */
	// if (! ((vram == 0x1000) || (vram == 0x4000) || (vram == 0x2000)) )
	//    return 1;

	//tms.model = model;
	tms.model = 1;	//tms9928a

	// Clear bitmap.
	memset(pixelData, 0, sizeof(pixelData));
	// All pixels are filled with zero bytes, so borders as well.
	memset(currBorderColours, 0, sizeof(currBorderColours));

	// Init line pointers array.
	for (int line = 0; line < HEIGHT; line++) {
		linePtrs[line] = pixelData + line * WIDTH;
	}

	// Video RAM
	tms.vramsize = 0x4000;
	tms.vMem = new byte[tms.vramsize];
	if (!tms.vMem) return ;//(1);
	memset(tms.vMem, 0, tms.vramsize);

	// Sprite back buffer
	tms.dBackMem = new byte[256 * 192];
	if (!tms.dBackMem) {
		free(tms.vMem);
		return ;//1;
	}

	// Dirty buffers
	tms.DirtyName = new byte[MAX_DIRTY_NAME];
	if (!tms.DirtyName) {
		free(tms.vMem);
		free(tms.dBackMem);
		return ;//1;
	}
	tms.DirtyPattern = new byte[MAX_DIRTY_PATTERN];
	if (!tms.DirtyPattern) {
		free(tms.vMem);
		free(tms.DirtyName);
		free(tms.dBackMem);
		return ;//1;
	}
	tms.DirtyColour =  new byte[MAX_DIRTY_COLOUR];
	if (!tms.DirtyColour) {
		free(tms.vMem);
		free(tms.DirtyName);
		free(tms.DirtyPattern);
		free(tms.dBackMem);
		return ;//1;
	}

	reset() ; // TMS9928A_reset ();
	tms.LimitSprites = 1;

	/* Open the display */
	//if(Verbose) printf("OK\n  Opening display...");
	PRT_DEBUG ("OK\n  Opening display...");
#ifdef FULLSCREEN
	if ((screen=SDL_SetVideoMode(WIDTH,HEIGHT,DEPTH,SDL_HWSURFACE|SDL_FULLSCREEN))==NULL)
#else
	if ((screen=SDL_SetVideoMode(WIDTH,HEIGHT,DEPTH,SDL_HWSURFACE))==NULL)
#endif
	{ printf("FAILED");return; }
	//{ if(Verbose) printf("FAILED");return; }

	// Hide mouse cursor
	SDL_ShowCursor(0);

	// Reset the palette
	for(int J=0;J<16;J++) {
		XPal[J] = SDL_MapRGB(screen->format,
			TMS9928A_palette[J*3],
			TMS9928A_palette[J*3 + 1],
			TMS9928A_palette[J*3 + 2]);
	}

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
}

void MSXTMS9928a::start()
{
	MSXDevice::start();
	//First interrupt in Pal mode here
	Scheduler::instance()->setSyncPoint(currentTime+71285, *this); // PAL
	putImage();
}

void MSXTMS9928a::executeUntilEmuTime(const Emutime &time)
{
	PRT_DEBUG("Executing TMS9928a at time " << time);

	//TODO: Change from full screen refresh to emutime based!!
	if (tms.stateChanged) {
		fullScreenRefresh();
		putImage();
		tms.stateChanged = false;
	}

	//Next SP/interrupt in Pal mode here
	currentTime = time;
	Scheduler::instance()->setSyncPoint(currentTime+71258, *this); //71285 for PAL, 59404 for NTSC
	// Since this is the vertical refresh
	tms.StatusReg |= 0x80;
	// Set interrupt if bits enable it
	if (tms.Regs[1] & 0x20) {
		setInterrupt();
	}
}

/*
~MSXTMS9928a()
{
    free (tms.vMem); tms.vMem = NULL;
    free (tms.dBackMem); tms.dBackMem = NULL;
    free (tms.DirtyColour); tms.DirtyColour = NULL;
    free (tms.DirtyName); tms.DirtyName = NULL;
    free (tms.DirtyPattern); tms.DirtyPattern = NULL;
}
*/

/** Set all dirty / clean
  */
void MSXTMS9928a::_TMS9928A_set_dirty(char dirty) {
    tms.anyDirtyColour = tms.anyDirtyName = tms.anyDirtyPattern = dirty;
    memset(tms.DirtyName, dirty, MAX_DIRTY_NAME);
    memset(tms.DirtyColour, dirty, MAX_DIRTY_COLOUR);
    memset(tms.DirtyPattern, dirty, MAX_DIRTY_PATTERN);
}

// The I/O functions.

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

			if ( (tms.Addr >= tms.nametbl)
			&& (tms.Addr < (tms.nametbl + MAX_DIRTY_NAME) ) ) {
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
				tms.stateChanged = true;
			}
			else {
				/* set read/write address */
				tms.Addr = ((word)value << 8 | tms.FirstByte) & (tms.vramsize - 1);
				if ( !(value & 0x40) ) {
					/* read ahead */
					TMS9928A_vram_r();
				}
			}
			tms.FirstByte = -1;
		}
		else {
			tms.FirstByte = value;
		}
		break;
	default:
		assert(false);
	}
}

byte MSXTMS9928a::TMS9928A_vram_r()
{
	//READ_HANDLER (TMS9928A_vram_r)
	byte ret = tms.ReadAhead;
	tms.ReadAhead = tms.vMem[tms.Addr];
	tms.Addr = (tms.Addr + 1) & (tms.vramsize - 1);
	tms.FirstByte = -1;
	return ret;
}

byte MSXTMS9928a::readIO(byte port, Emutime &time)
{
	switch (port) {
	case 0x98:
		return TMS9928A_vram_r();
	case 0x99: {
		//READ_HANDLER (TMS9928A_register_r)
		byte ret = tms.StatusReg;
		tms.StatusReg &= 0x5f;
		tms.FirstByte = -1;
        resetInterrupt();
		return ret;
	}
	default:
		assert(false);
	}
}

void MSXTMS9928a::_TMS9928A_change_register(byte reg, byte val)
{
	static const byte Mask[8] =
		{ 0x03, 0xfb, 0x0f, 0xff, 0x07, 0x7f, 0x07, 0xff };
	static const char *modes[] = {
		"Mode 0 (GRAPHIC 1)", "Mode 1 (TEXT 1)", "Mode 2 (GRAPHIC 2)",
		"Mode 1+2 (TEXT 1 variation)", "Mode 3 (MULTICOLOR)",
		"Mode 1+3 (BOGUS)", "Mode 2+3 (MULTICOLOR variation)",
		"Mode 1+2+3 (BOGUS)" };

	val &= Mask[reg];
	byte oldval = tms.Regs[reg];
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
			}
			else {
				tms.colour = (tms.Regs[3] * 64) & (tms.vramsize - 1);
				tms.pattern = (tms.Regs[4] * 2048) & (tms.vramsize - 1);
			}
			tms.mode = TMS_MODE;
			//PRT_DEBUG ("TMS9928A: now in mode " << tms.mode );
			//PRT_DEBUG (sprintf("TMS9928A: %s\n", modes[tms.mode]));
			_TMS9928A_set_dirty(1);
		}
		break;
	case 1: {
		// check for changes in the INT line
		if ((val & 0x20) && (tms.StatusReg & 0x80)) {
			/* Set the interrupt line !! */
			setInterrupt();
		}
		int mode = TMS_MODE;
		if (tms.mode != mode) {
			tms.mode = mode;
			_TMS9928A_set_dirty(1);
			printf("TMS9928A: %s\n", modes[tms.mode]);
			//PRT_DEBUG (sprintf("TMS9928A: %s\n", modes[tms.mode]));
		}
		break;
	}
	case 2:
		tms.nametbl = (val * 1024) & (tms.vramsize - 1);
		tms.anyDirtyName = 1;
		memset(tms.DirtyName, 1, MAX_DIRTY_NAME);
		break;
	case 3:
		if (tms.Regs[0] & 2) {
			tms.colour = ((val & 0x80) * 64) & (tms.vramsize - 1);
			tms.colourmask = (val & 0x7f) * 8 | 7;
		}
		else {
			tms.colour = (val * 64) & (tms.vramsize - 1);
		}
		tms.anyDirtyColour = 1;
		memset (tms.DirtyColour, 1, MAX_DIRTY_COLOUR);
		break;
	case 4:
		if (tms.Regs[0] & 2) {
			tms.pattern = ((val & 4) * 2048) & (tms.vramsize - 1);
			tms.patternmask = (val & 3) * 256 | 255;
		}
		else {
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
		// Any line containing transparent pixels must be repainted.
		// We don't know which lines contain transparent pixels,
		// so we have to repaint them all.
		// TODO: Maybe this can be optimised for some screen modes.
		tms.anyDirtyColour = 1;
		memset(tms.DirtyColour, 1, MAX_DIRTY_COLOUR);
		break;
	}
}

