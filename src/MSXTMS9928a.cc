// $Id$

/*
Implementation of the Texas Instruments TMS9918A and TMS9928A.


Line-based sprite code by Maarten ter Huurne.
OpenMSX integration by David Heremans.
Based on Sean Young's code from MESS,
which was in turn based on code by Mike Balfour.


All undocumented features as described in the following file
should be emulated.

http://www.msxnet.org/tech/tms9918a.txt


TODO:
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


#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "SDLLoRenderer.hh"
#include "MSXTMS9928a.hh"
#include "config.h"

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
const byte MSXTMS9928a::TMS9928A_PALETTE[16 * 3] =
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

/** Fill a boolean array with a single value.
  * Optimised for byte-sized booleans,
  * but correct for every size.
  */
// TODO: Routine is duplicated in SDLLoRenderer.
inline static void fillBool(bool *ptr, bool value, int nr)
{
#if SIZEOF_BOOL == 1
	memset(ptr, value, nr);
#else
	for (int i = nr; i--; ) *ptr++ = value;
#endif
}

// TODO: Routine is duplicated in SDLLoRenderer.
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
	// TODO: Move as much code as possible from init() to here.
}

MSXTMS9928a::~MSXTMS9928a()
{
	PRT_DEBUG("Destroying an MSXTMS9928a object");
	delete(renderer);
	delete(tms.vMem);
}

// The init, start, reset and shutdown functions

void MSXTMS9928a::reset()
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
	renderer = createSDLLoRenderer(this, fullScreen);

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

