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
- Apply line-based scheduling.
- Sprite attribute readout probably happens one line in advance.
  This matters when line-based scheduling is operational.
- Get rid of hardcoded port 0x98 and 0x99.
*/


#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "SDLLoRenderer.hh"
#include "MSXTMS9928a.hh"

#include <string>
#include <cassert>


// TODO: Change into enum or get rid of it.
#define TMS99x8A 1

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

int MSXTMS9928a::doublePattern(int pattern)
{
	// Double every dot.
	int ret = 0;
	for (int i = 16; i--; ) {
		ret <<= 2;
		if (pattern & 0x80000000) {
			ret |= 3;
		}
		pattern <<= 1;
	}
	return ret;
}

int MSXTMS9928a::checkSprites(int line, int *visibleSprites)
{
	// Get sprites for this line and detect 5th sprite if any.
	int sprite, visibleIndex = 0;
	int size = getSpriteSize();
	int magSize = size * (getSpriteMag() + 1);
	int minStart = line - magSize;
	byte *attributePtr = spriteAttributeBasePtr;
	for (sprite = 0; sprite < 32; sprite++, attributePtr += 4) {
		int y = *attributePtr;
		if (y == 208) break;
		y = (y > 208 ? y - 255 : y + 1);
		if ((y > minStart) && (y <= line)) {
			if (visibleIndex == 4) {
				// Five sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero.
				if (~statusReg & 0xC0) {
					statusReg = (statusReg & 0xE0) | 0x40 | sprite;
				}
				if (limitSprites) break;
			}
			else {
				visibleSprites[visibleIndex++] = sprite;
			}
		}
	}
	if (~statusReg & 0x40) {
		// No 5th sprite detected, store number of latest sprite processed.
		statusReg = (statusReg & 0xE0) | (sprite < 32 ? sprite : 31);
	}

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The visibleSprites array is filled now, so we can bail out.
	if (statusReg & 0x20) return visibleIndex;

	/*
	Model for sprite collision: (or "coincidence" in TMS9918 data sheet)
	Reset when status reg is read.
	Set when sprite patterns overlap.
	Colour doesn't matter: sprites of colour 0 can collide.
	Sprites with off-screen position can collide.

	Implemented by checking every pair for collisions.
	For large numbers of sprites that would be slow,
	but there are max 4 sprites and therefore max 6 pairs.
	If any collision is found, method returns at once.
	*/
	for (int i = (visibleIndex < 4 ? visibleIndex : 4); --i >= 1; ) {
		byte *attributePtr = spriteAttributeBasePtr + visibleSprites[i] * 4;
		int y_i = *attributePtr++;
		int x_i = *attributePtr++;
		int patternIndex_i =
			((size == 16) ? *attributePtr & 0xFC : *attributePtr);
		if ((*++attributePtr) & 0x80) x_i -= 32;

		for (int j = i; --j >= 0; ) {
			attributePtr = spriteAttributeBasePtr + visibleSprites[j] * 4;
			int y_j = *attributePtr++;
			int x_j = *attributePtr++;
			int patternIndex_j =
				((size == 16) ? *attributePtr & 0xFC : *attributePtr);
			if ((*++attributePtr) & 0x80) x_j -= 32;

			// Do sprite i and sprite j collide?
			int dist = x_j - x_i;
			if ((-magSize < dist) && (dist < magSize)) {
				int pattern_i = calculatePattern(patternIndex_i, line - y_i);
				int pattern_j = calculatePattern(patternIndex_j, line - y_j);
				if (dist < 0) {
					pattern_i >>= -dist;
				}
				else if (dist > 0) {
					pattern_j >>= dist;
				}
				if (pattern_i & pattern_j) {
					// Collision!
					statusReg |= 0x20;
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
	delete[](vramData);
}

// The init, start, reset and shutdown functions

void MSXTMS9928a::reset()
{
	MSXDevice::reset();

	for (int i = 0; i < 8; i++) controlRegs[i] = 0;
	statusReg = 0;
	vramPointer = 0;
	readAhead = 0;
	firstByte = -1;

	spriteAttributeBasePtr = vramData;
	spritePatternBasePtr = vramData;

	// TODO: Reset the renderer.
}

void MSXTMS9928a::init()
{
	MSXDevice::init();

	limitSprites = true; // TODO: Read from config.

	model = TMS99x8A;

	// Video RAM
	int vramSize = 0x4000;
	vramMask = vramSize - 1;
	vramData = new byte[vramSize];
	// TODO: Use exception instead?
	if (!vramData) return ;//1;
	memset(vramData, 0, vramSize);

	reset();

	// TODO: Move Renderer creation outside of this class.
	//   A setRenderer method would be used to provide a renderer.
	//   It should be possible to switch the Renderer at run time,
	//   probably on user request.
	fullScreen = atoi(deviceConfig->getParameter("fullscreen").c_str());
	renderer = createSDLLoRenderer(this, fullScreen);

	// Register hotkey for fullscreen togling
	HotKey::instance()->registerAsyncHotKey(SDLK_PRINT, this);

	MSXMotherBoard::instance()->register_IO_In((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_In((byte)0x99, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x99, this);

	return;
}

void MSXTMS9928a::signalHotKey(SDLKey key)
{
	// Only key currently registered is full screen toggle.
	fullScreen = !fullScreen;
	renderer->setFullScreen(fullScreen);
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

TODO:
Currently two things are done:
- status register bookkeeping
- force render
The status registers will probably be calculated on demand in the
future, instead of being kept current all the time. For registers
like HR it will save the scheduler a lot of effort if they are
calculated on demand.
So only the rendering remains. Then it's probably better to schedule
the renderer directly, instead of through the VDP.
*/
void MSXTMS9928a::executeUntilEmuTime(const Emutime &time)
{
	PRT_DEBUG("Executing TMS9928a at time " << time);

	renderer->putImage();

	//Next SP/interrupt in Pal mode here
	currentTime = time;
	Scheduler::instance()->setSyncPoint(currentTime+71258, *this); //71285 for PAL, 59404 for NTSC
	// Since this is the vertical refresh
	statusReg |= 0x80;
	// Set interrupt if bits enable it
	if (controlRegs[1] & 0x20) {
		setInterrupt();
	}
}

// The I/O functions.

void MSXTMS9928a::writeIO(byte port, byte value, Emutime &time)
{
	switch (port){
	case 0x98: {
		if (vramData[vramPointer] != value) {
			vramData[vramPointer] = value;
			renderer->updateVRAM(vramPointer, value, time);
		}
		vramPointer = (vramPointer + 1) & vramMask;
		readAhead = value;
		firstByte = -1;
		break;
	}
	case 0x99:
		if (firstByte != -1) {
			if (value & 0x80) {
				// Register write.
				changeRegister((int)(value & 7), firstByte, time);
			}
			else {
				// Set read/write address.
				vramPointer = ((word)value << 8 | firstByte) & vramMask;
				if ( !(value & 0x40) ) {
					// Read ahead.
					vramRead();
				}
			}
			firstByte = -1;
		}
		else {
			firstByte = value;
		}
		break;
	default:
		assert(false);
	}
}

byte MSXTMS9928a::vramRead()
{
	byte ret = readAhead;
	readAhead = vramData[vramPointer];
	vramPointer = (vramPointer + 1) & vramMask;
	firstByte = -1;
	return ret;
}

byte MSXTMS9928a::readIO(byte port, Emutime &time)
{
	switch (port) {
	case 0x98:
		return vramRead();
	case 0x99: {
		byte ret = statusReg;
		// TODO: Used to be 0x5f, but that is contradicted by TMS9918.pdf
		statusReg &= 0x1f;
		firstByte = -1;
		resetInterrupt();
		return ret;
	}
	default:
		assert(false);
	}
}

void MSXTMS9928a::changeRegister(byte reg, byte val, Emutime &time)
{
	static const byte REG_MASKS[8] =
		{ 0x03, 0xfb, 0x0f, 0xff, 0x07, 0x7f, 0x07, 0xff };

	val &= REG_MASKS[reg];
	byte oldval = controlRegs[reg];
	if (oldval == val) return;
	controlRegs[reg] = val;

	PRT_DEBUG("TMS9928A: Reg " << (int)reg << " = " << (int)val);
	switch (reg) {
	case 0:
		if ((val ^ oldval) & 2) {
			// Re-calculate masks and pattern generator & colour.
			if (val & 2) {
				int colourMask = (controlRegs[3] & 0x7f) * 8 | 7;
				renderer->updateColourBase(
					((controlRegs[3] & 0x80) * 64) & vramMask,
					colourMask,
					time
					);
				renderer->updatePatternBase(
					((controlRegs[4] & 4) * 2048) & vramMask,
					(controlRegs[4] & 3) * 256 | (colourMask & 255),
					time
					);
			}
			else {
				renderer->updateColourBase(
					(controlRegs[3] * 64) & vramMask, -1, time);
				renderer->updatePatternBase(
					(controlRegs[4] * 2048) & vramMask, -1, time);
			}
			updateDisplayMode(time);
		}
		break;
	case 1:
		// check for changes in the INT line
		if ((val & 0x20) && (statusReg & 0x80)) {
			/* Set the interrupt line !! */
			setInterrupt();
		}
		updateDisplayMode(time);
		break;
	case 2:
		renderer->updateNameBase((val * 1024) & vramMask, time);
		break;
	case 3:
		if (controlRegs[0] & 2) {
			renderer->updateColourBase(
				((val & 0x80) * 64) & vramMask,
				(val & 0x7f) * 8 | 7,
				time
				);
		}
		else {
			renderer->updateColourBase((val * 64) & vramMask, -1, time);
		}
		break;
	case 4:
		if (controlRegs[0] & 2) {
			renderer->updatePatternBase(
				((val & 4) * 2048) & vramMask,
				(val & 3) * 256 | 255,
				time
				);
		}
		else {
			renderer->updatePatternBase((val * 2048) & vramMask, -1, time);
		}
		break;
	case 5: {
		int addr = (val * 128) & vramMask;
		spriteAttributeBasePtr = vramData + addr;
		renderer->updateSpriteAttributeBase(addr, time);
		break;
	}
	case 6: {
		int addr = (val * 2048) & vramMask;
		spritePatternBasePtr = vramData + addr;
		renderer->updateSpritePatternBase(addr, time);
		break;
	}
	case 7:
		renderer->updateBackgroundColour(val & 0x0F, time);
		break;
	}
}

void MSXTMS9928a::updateDisplayMode(Emutime &time)
{
	static const char *MODE_STRINGS[] = {
		"Mode 0 (GRAPHIC 1)", "Mode 1 (TEXT 1)", "Mode 2 (GRAPHIC 2)",
		"Mode 1+2 (TEXT 1 variation)", "Mode 3 (MULTICOLOR)",
		"Mode 1+3 (BOGUS)", "Mode 2+3 (MULTICOLOR variation)",
		"Mode 1+2+3 (BOGUS)" };

	int newMode =
		  (model == TMS99x8A ? (controlRegs[0] & 2) : 0)
		| ((controlRegs[1] & 0x10) >> 4)
		| ((controlRegs[1] & 0x08) >> 1);
	if (newMode != displayMode) {
		displayMode = newMode;
		renderer->updateDisplayMode(newMode, time);
		printf("TMS9928A: mode %s\n", MODE_STRINGS[newMode]);
	}
}

