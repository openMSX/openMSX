// $Id$

/* TODO:
- Verify the dirty checks, especially those of mode3 and mode23,
  which were different before.
- Apply line-based scheduling.
- Sprite attribute readout probably happens one line in advance.
  This matters when line-based scheduling is operational.

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

// TODO: Read this information from config.h.
#define SIZEOF_BOOL 1
/** Fill a boolean array with a single value.
  * Optimised for byte-sized booleans,
  * but correct for every size.
  */
inline static void fillBool(bool *ptr, bool value, int nr)
{
#if SIZEOF_BOOL == 1
	memset(ptr, value, nr);
#else
	for (int i = nr; i--; ) *ptr++ = value;
#endif
}

SDLRenderer::RenderMethod SDLRenderer::modeToRenderMethod[] = {
	&SDLRenderer::mode0,
	&SDLRenderer::mode1,
	&SDLRenderer::mode2,
	&SDLRenderer::mode12,
	&SDLRenderer::mode3,
	&SDLRenderer::modebogus,
	&SDLRenderer::mode23,
	&SDLRenderer::modebogus
};

inline static int calculatePattern(byte *patternPtr, int y, int size, int mag)
{
	// Calculate pattern.
	if (mag) y /= 2;
	int pattern = patternPtr[y] << 24;
	if (size == 16) {
		pattern |= patternPtr[y + 16] << 16;
	}
	if (mag) {
		// Double every dot.
		int doublePattern = 0;
		for (int i = 16; i--; ) {
			doublePattern <<= 2;
			if (pattern & 0x80000000) {
				doublePattern |= 3;
			}
			pattern <<= 1;
		}
		return doublePattern;
	}
	else {
		return pattern;
	}
}

int MSXTMS9928a::checkSprites(
	int line, int *visibleSprites, int size, int mag)
{
	// Get sprites for this line and detect 5th sprite if any.
	int sprite, visibleIndex = 0;
	int magSize = size * (mag + 1);
	int minStart = line - magSize;
	byte *attributePtr = tms.vMem + tms.spriteattribute;
	for (sprite = 0; sprite < 32; sprite++, attributePtr += 4) {
		int y = *attributePtr;
		if (y == 208) break;
		y = (y > 208 ? y - 255 : y + 1);
		if ((y > minStart) && (y <= line)) {
			if (visibleIndex == 4) {
				// Five sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero.
				if (~tms.StatusReg & 0xC0) {
					tms.StatusReg = (tms.StatusReg & 0xE0) | 0x40 | sprite;
				}
				if (limitSprites) break;
			}
			else {
				visibleSprites[visibleIndex++] = sprite;
			}
		}
	}
	if (~tms.StatusReg & 0x40) {
		// No 5th sprite detected, store number of latest sprite processed.
		tms.StatusReg = (tms.StatusReg & 0xE0) | (sprite < 32 ? sprite : 31);
	}

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The visibleSprites array is filled now, so we can bail out.
	if (tms.StatusReg & 0x20) return visibleIndex;

	/*
	Model for sprite collision: (or "coincidence" in TMS9918 data sheet)
	Reset when status reg is read.
	Set when sprite patterns overlap.
	Colour doesn't matter: sprites of colour 0 can collide.
	Sprites with off-screen position can collide.

	Implemented by checking every pair for collisions.
	For large numbers of sprites that would be slow,
	but there are max 4 sprites and max 6 pairs.
	If any collision is found, method returns at once.
	*/
	for (int i = (visibleIndex < 4 ? visibleIndex : 4); --i >= 1; ) {
		byte *attributePtr = tms.vMem + tms.spriteattribute +
			visibleSprites[i] * 4;
		int y_i = *attributePtr++;
		int x_i = *attributePtr++;
		byte *patternPtr_i = tms.vMem + tms.spritepattern +
			((size == 16) ? *attributePtr & 0xFC : *attributePtr) * 8;
		if ((*++attributePtr) & 0x80) x_i -= 32;

		for (int j = i; --j >= 0; ) {
			attributePtr = tms.vMem + tms.spriteattribute +
				visibleSprites[j] * 4;
			int y_j = *attributePtr++;
			int x_j = *attributePtr++;
			byte *patternPtr_j = tms.vMem + tms.spritepattern +
				((size == 16) ? *attributePtr & 0xFC : *attributePtr) * 8;
			if ((*++attributePtr) & 0x80) x_j -= 32;

			// Do sprite i and sprite j collide?
			int dist = x_j - x_i;
			if ((-magSize < dist) && (dist < magSize)) {
				int pattern_i = calculatePattern(patternPtr_i, line - y_i, size, mag);
				int pattern_j = calculatePattern(patternPtr_j, line - y_j, size, mag);
				if (dist < 0) {
					pattern_i >>= -dist;
				}
				else if (dist > 0) {
					pattern_j >>= dist;
				}
				if (pattern_i & pattern_j) {
					// Collision!
					tms.StatusReg |= 0x20;
					return visibleIndex;
				}
			}
		}
	}

	return visibleIndex;
}

MSXTMS9928a::MSXTMS9928a(MSXConfig::Device *config) : MSXDevice(config), currentTime(3579545, 0)
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
	setDirty(true);
	stateChanged = true;
}

void MSXTMS9928a::init()
{
	MSXDevice::init();

	limitSprites = true; // TODO: Read from config.

	tms.model = 1;	//tms9928a

	// Video RAM
	tms.vramsize = 0x4000;
	tms.vMem = new byte[tms.vramsize];
	// TODO: Use exception instead?
	if (!tms.vMem) return ;//1;
	memset(tms.vMem, 0, tms.vramsize);

	reset();

	printf("fullscreen = [%s]\n", deviceConfig->getParameter("fullscreen").c_str());
	bool fullScreen = atoi(deviceConfig->getParameter("fullscreen").c_str());
	renderer = new SDLRenderer(this, fullScreen);

	// Register hotkey for fullscreen togling
	HotKey::instance()->registerAsyncHotKey(SDLK_PRINT,this);

	MSXMotherBoard::instance()->register_IO_In((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_In((byte)0x99, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x99, this);

	return;
}

void MSXTMS9928a::signalHotKey(SDLKey key)
{
	// We don't care which key, since we only registered one.
	renderer->toggleFullScreen();
}

void MSXTMS9928a::start()
{
	MSXDevice::start();
	//First interrupt in Pal mode here
	Scheduler::instance()->setSyncPoint(currentTime+71285, *this); // PAL
	renderer->putImage();
}

/*
TODO:
Code seems to assume VDP is called at 50/60Hz.
But is it guaranteed that the set sync points are actually the only
times a sync is done?
It seems to me that status reg reads can also be a reason for syncing.
If not currently, then certainly in the future.
*/
void MSXTMS9928a::executeUntilEmuTime(const Emutime &time)
{
	PRT_DEBUG("Executing TMS9928a at time " << time);

	//TODO: Change from full screen refresh to emutime based!!
	if (stateChanged) {
		renderer->fullScreenRefresh();
		renderer->putImage();
		stateChanged = false;
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

void MSXTMS9928a::setDirty(bool dirty)
{
	anyDirtyColour = anyDirtyPattern = anyDirtyName = dirty;
	fillBool(dirtyName, dirty, sizeof(dirtyName));
	fillBool(dirtyColour, dirty, sizeof(dirtyColour));
	fillBool(dirtyPattern, dirty, sizeof(dirtyPattern));
}

// The I/O functions.

void MSXTMS9928a::writeIO(byte port, byte value, Emutime &time)
{
	switch (port){
	case 0x98: {
		//WRITE_HANDLER (TMS9928A_vram_w)
		if (tms.vMem[tms.Addr] != value) {
			tms.vMem[tms.Addr] = value;
			tms.Change = 1;
			/* dirty optimization */
			stateChanged = true;

			if ( (tms.Addr >= tms.nametbl)
			&& (tms.Addr < (tms.nametbl + (int)sizeof(dirtyName)) ) ) {
				dirtyName[tms.Addr - tms.nametbl] = anyDirtyName = true;
			}

			int i;
			i = (tms.Addr - tms.colour) >> 3;
			if ( (i >= 0) && (i < (int)sizeof(dirtyColour)) ) {
				dirtyColour[i] = anyDirtyColour = true;

			}

			i = (tms.Addr - tms.pattern) >> 3;
			if ( (i >= 0) && (i < (int)sizeof(dirtyPattern)) ) {
				dirtyPattern[i] = anyDirtyPattern = true;
			}
		}
		tms.Addr = (tms.Addr + 1) & (tms.vramsize - 1);
		tms.ReadAhead = value;
		tms.FirstByte = -1;
		break;
	}
	case 0x99:
		//WRITE_HANDLER (TMS9928A_register_w)
		if (tms.FirstByte >= 0) {
			if (value & 0x80) {
				/* register write */
				_TMS9928A_change_register ((int)(value & 7), tms.FirstByte);
				stateChanged = true;
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
		// TODO: Used to be 0x5f, but that is contradicted by TMS9918.pdf
		tms.StatusReg &= 0x1f;
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
			setDirty(true);
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
			setDirty(true);
			printf("TMS9928A: %s\n", modes[tms.mode]);
			//PRT_DEBUG (sprintf("TMS9928A: %s\n", modes[tms.mode]));
		}
		break;
	}
	case 2:
		tms.nametbl = (val * 1024) & (tms.vramsize - 1);
		anyDirtyName = true;
		fillBool(dirtyName, true, sizeof(dirtyName));
		break;
	case 3:
		if (tms.Regs[0] & 2) {
			tms.colour = ((val & 0x80) * 64) & (tms.vramsize - 1);
			tms.colourmask = (val & 0x7f) * 8 | 7;
		}
		else {
			tms.colour = (val * 64) & (tms.vramsize - 1);
		}
		anyDirtyColour = true;
		fillBool(dirtyColour, true, sizeof(dirtyColour));
		break;
	case 4:
		if (tms.Regs[0] & 2) {
			tms.pattern = ((val & 4) * 2048) & (tms.vramsize - 1);
			tms.patternmask = (val & 3) * 256 | 255;
		}
		else {
			tms.pattern = (val * 2048) & (tms.vramsize - 1);
		}
		anyDirtyPattern = true;
		fillBool(dirtyPattern, true, sizeof(dirtyPattern));
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
		anyDirtyColour = true;
		fillBool(dirtyColour, true, sizeof(dirtyColour));
		break;
	}
}

// ========================================================================

SDLRenderer::SDLRenderer(MSXTMS9928a *vdp, bool fullScreen)
{
	this->vdp = vdp;

	// Clear bitmap.
	memset(pixelData, 0, sizeof(pixelData));
	// All pixels are filled with zero bytes, so borders as well.
	memset(currBorderColours, 0, sizeof(currBorderColours));

	// Init line pointers array.
	for (int line = 0; line < HEIGHT; line++) {
		linePtrs[line] = pixelData + line * WIDTH;
	}

	/* Open the display */
	PRT_DEBUG("OK\n  Opening display...");
	screen = SDL_SetVideoMode( WIDTH, HEIGHT, DEPTH,
		SDL_HWSURFACE | ( fullScreen ? SDL_FULLSCREEN : 0) );
	if (screen == NULL) {
		printf("FAILED");
		return;
	}

	// Hide mouse cursor
	SDL_ShowCursor(0);

	// Reset the palette
	for (int i = 0; i < 16; i++) {
		XPal[i] = SDL_MapRGB(screen->format,
			TMS9928A_palette[i * 3 + 0],
			TMS9928A_palette[i * 3 + 1],
			TMS9928A_palette[i * 3 + 2]);
	}

}

/*
SDLRenderer::~SDLRenderer()
{
}
*/

void SDLRenderer::toggleFullScreen()
{
	SDL_WM_ToggleFullScreen(screen);
}

// RENDERERS for line-based scanline conversion

void SDLRenderer::mode0(Pixel *pixelPtr, int line)
{
	Pixel backDropColour = XPal[vdp->tms.Regs[7] & 0x0F];
	int name = (line / 8) * 32;
	// TODO: charLefts may have slightly better performance,
	// but x may be easier to convert to pixel-based rendering.
	for (int charsLeft = 32; charsLeft--; ) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl + name];
		// TODO: is dirtyColour[charcode / 64] correct?
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]
		|| vdp->dirtyColour[charcode / 64]) {

			int colour = vdp->tms.vMem[vdp->tms.colour + charcode / 8];
			Pixel fg = colour >> 4;   fg = (fg ? XPal[fg] : backDropColour);
			Pixel bg = colour & 0x0F; bg = (bg ? XPal[bg] : backDropColour);

			int pattern = vdp->tms.vMem[vdp->tms.pattern + charcode * 8 + (line & 0x07)];
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

void SDLRenderer::mode1(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;
	if (!vdp->anyDirtyColour) return;

	Pixel fg = XPal[vdp->tms.Regs[7] >> 4];
	Pixel bg = XPal[vdp->tms.Regs[7] & 0x0F];

	int name = (line / 8) * 40;
	int x = 0;
	// Extended left border.
	for (; x < 8; x++) *pixelPtr++ = bg;
	// Actual display.
	while (x < 248) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl + name];
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]) {
			int pattern = vdp->tms.vMem[vdp->tms.pattern + charcode * 8 + (line & 7)];
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

void SDLRenderer::mode2(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;

	Pixel backDropColour = XPal[vdp->tms.Regs[7] & 0x0F];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl+name] + (line / 64) * 256;
		int colourNr = (charcode & vdp->tms.colourmask);
		int patternNr = (charcode & vdp->tms.patternmask);
		if (vdp->dirtyName[name] || vdp->dirtyPattern[patternNr]
		|| vdp->dirtyColour[colourNr]) {
			// TODO: pattern uses colourNr and colour uses patterNr...
			//      I don't get it.
			int pattern = vdp->tms.vMem[vdp->tms.pattern + colourNr * 8 + (line & 7)];
			int colour = vdp->tms.vMem[vdp->tms.colour + patternNr * 8 + (line & 7)];
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

void SDLRenderer::mode12(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;
	if (!vdp->anyDirtyColour) return;

	Pixel fg = XPal[vdp->tms.Regs[7] >> 4];
	Pixel bg = XPal[vdp->tms.Regs[7] & 0x0F];

	int name = (line / 8) * 32;
	int x = 0;
	// Extended left border.
	for ( ; x < 8; x++) *pixelPtr++ = bg;
	// Actual display.
	while (x < 248) {
		int charcode = (vdp->tms.vMem[vdp->tms.nametbl + name] + (line / 64) * 256) & vdp->tms.patternmask;
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]) {
			int pattern = vdp->tms.vMem[vdp->tms.pattern + charcode * 8];
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

void SDLRenderer::mode3(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;
	if (!vdp->anyDirtyColour) return;

	Pixel backDropColour = XPal[vdp->tms.Regs[7] & 0x0F];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl + name];
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]) {
			int colour = vdp->tms.vMem[vdp->tms.pattern + charcode * 8 + ((line / 4) & 7)];
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

void SDLRenderer::modebogus(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;

	Pixel fg = XPal[vdp->tms.Regs[7] >> 4];
	Pixel bg = XPal[vdp->tms.Regs[7] & 0x0F];
	int x = 0;
	for (; x < 8; x++) *pixelPtr++ = bg;
	for (; x < 248; x += 6) {
		int n;
		for (n = 4; n--; ) *pixelPtr++ = fg;
		for (n = 2; n--; ) *pixelPtr++ = bg;
	}
	for (; x < 256; x++) *pixelPtr++ = bg;
}

void SDLRenderer::mode23(Pixel *pixelPtr, int line)
{
	// Not needed since full screen refresh not executed now
	//if ( !(vdp->anyDirtyColour || vdp->anyDirtyName || vdp->anyDirtyPattern) )
	//  return;
	if (!vdp->anyDirtyColour) return;

	Pixel backDropColour = XPal[vdp->tms.Regs[7] & 0x0F];
	int name = (line / 8) * 32;
	for (int x = 0; x < 256; x += 8) {
		int charcode = vdp->tms.vMem[vdp->tms.nametbl + name];
		if (vdp->dirtyName[name] || vdp->dirtyPattern[charcode]) {
			int colour = vdp->tms.vMem[vdp->tms.pattern + ((charcode + ((line / 4) & 7) +
				(line / 64) * 256) & vdp->tms.patternmask) * 8];
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
void SDLRenderer::modeblank(Pixel *pixelPtr, int line)
{
	// Screen blanked so all background colour.
	Pixel colour = XPal[vdp->tms.Regs[7] & 0x0F];
	for (int x = 0; x < 256; x++) {
		*pixelPtr++ = colour;
	}
}

bool SDLRenderer::drawSprites(Pixel *pixelPtr, int line, bool *dirty)
{
	int size = (vdp->tms.Regs[1] & 2) ? 16 : 8;
	int mag = vdp->tms.Regs[1] & 1; // 0 = normal, 1 = double

	// Determine sprites visible on this line.
	// Also sets status reg properly.
	int visibleSprites[32];
	int visibleIndex = vdp->checkSprites(line, visibleSprites, size, mag);

	bool ret = false;
	while (visibleIndex--) {
		// Get sprite info.
		byte *attributePtr = vdp->tms.vMem + vdp->tms.spriteattribute +
			visibleSprites[visibleIndex] * 4;
		int y = *attributePtr++;
		y = (y > 208 ? y - 255 : y + 1);
		int x = *attributePtr++;
		byte *patternPtr = vdp->tms.vMem + vdp->tms.spritepattern +
			((size == 16) ? *attributePtr & 0xFC : *attributePtr) * 8;
		Pixel colour = *++attributePtr;
		if (colour & 0x80) x -= 32;
		colour &= 0x0F;
		if (colour == 0) {
			// Don't draw transparent sprites.
			continue;
		}
		colour = XPal[colour];

		int pattern = calculatePattern(patternPtr, line - y, size, mag);

		// Skip any dots that end up in the left border.
		if (x < 0) {
			pattern <<= -x;
			x = 0;
		}
		// Sprites are only visible in screen modes which have lines
		// of 32 8x8 chars.
		bool *dirtyPtr = dirty + (x / 8);
		if (pattern) {
			vdp->anyDirtyName = true;
			ret = true;
		}
		// Convert pattern to pixels.
		bool charDirty = false;
		while (pattern && (x < 256)) {
			// Draw pixel if sprite has a dot.
			if (pattern & 0x80000000) {
				pixelPtr[x] = colour;
				charDirty = true;
			}
			// Advancing behaviour.
			pattern <<= 1;
			x++;
			if ((x & 7) == 0) {
				if (charDirty) *dirtyPtr = true;
				charDirty = false;
				dirtyPtr++;
			}
		}
		// Semi-filled characters can be dirty as well.
		if ((x < 256) && charDirty) *dirtyPtr = true;
	}

	return ret;
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

void SDLRenderer::fullScreenRefresh()
{
	// Only redraw if needed.
	if (!vdp->tms.Change) return;

	Pixel borderColour = XPal[vdp->tms.Regs[7] & 0x0F];
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

	// Characters that contain sprites should be drawn again next frame.
	bool nextDirty[32];
	bool nextAnyDirty = false; // dirty pixel in current character row?
	bool nextAnyDirtyName = false; // dirty pixel anywhere in frame?

	// Display area.
	RenderMethod renderMethod = modeToRenderMethod[vdp->tms.mode];
	for (int y = 0; y < 192; y++, line++, currBorderColoursPtr++) {
		if ((y & 7) == 0) {
			fillBool(nextDirty, false, 32);
			nextAnyDirty = false;
		}
		Pixel *linePtr = &linePtrs[line][displayX];
		if (vdp->tms.Regs[1] & 0x40) {
			// Draw background.
			(this->*renderMethod)(linePtr, y);
			// Sprites enabled?
			if ((vdp->tms.Regs[1] & 0x50) == 0x40) {
				nextAnyDirty |= drawSprites(linePtr, y, nextDirty);
			}
		}
		else {
			modeblank(linePtr, y);
		}
		if ((y & 7) == 7) {
			if (nextAnyDirty) {
				memcpy(vdp->dirtyName + (y / 8) * 32, nextDirty, 32);
				nextAnyDirtyName = true;
			}
			else {
				fillBool(vdp->dirtyName + (y / 8) * 32, false, 32);
			}
		}
		// Borders are drawn after the display area:
		// V9958 can extend the left border over the display area,
		// this is implemented using overdraw.
		// TODO: Does the extended border clip sprites as well?
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
	vdp->anyDirtyName = nextAnyDirtyName;
	vdp->anyDirtyColour = vdp->anyDirtyPattern = false;
	fillBool(vdp->dirtyColour, false, sizeof(vdp->dirtyColour));
	fillBool(vdp->dirtyPattern, false, sizeof(vdp->dirtyPattern));
}

void SDLRenderer::putImage()
{
	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen)<0) return;//ExitNow=1;

	// Copy image.
	memcpy(screen->pixels, pixelData, sizeof(pixelData));

	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

	// Update screen.
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

