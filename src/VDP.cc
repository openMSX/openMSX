// $Id$

/*
TODO:
- Apply line-based scheduling.
- Sprite attribute readout probably happens one line in advance.
  This matters when line-based scheduling is operational.
- Get rid of hardcoded port 0x98 and 0x99.
*/


#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "SDLLoRenderer.hh"
#include "SDLHiRenderer.hh"
#include "VDP.hh"

#include <SDL/SDL.h>

#include <string>
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
const byte VDP::TMS9928A_PALETTE[16 * 3] =
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

VDP::VDP(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating a VDP object");

	limitSprites = deviceConfig->getParameterAsBool("limit_sprites");

	std::string versionString = deviceConfig->getParameter("version");
	if (versionString == "TMS99X8A") version = TMS99X8A;
	else if (versionString == "TMS9929A") version = TMS9929A;
	else if (versionString == "V9938") version = V9938;
	else if (versionString == "V9958") version = V9958;
	else PRT_ERROR("Unknown VDP version \"" << versionString << "\"");

	controlRegMask = (version == V9938 || version == V9958 ? 0x3F : 0x07);

	// Video RAM.
	int vramSize = 0x4000;
	vramMask = vramSize - 1;
	vramData = new byte[vramSize];
	// TODO: Use exception instead?
	if (!vramData) return ;//1;
	memset(vramData, 0, vramSize);

	reset(time);

	// TODO: Move Renderer creation outside of this class.
	//   A setRenderer method would be used to provide a renderer.
	//   It should be possible to switch the Renderer at run time,
	//   probably on user request.
	MSXConfig::Config *renderConfig =
		MSXConfig::instance()->getConfigById("renderer");
	fullScreen = renderConfig->getParameterAsBool("full_screen");
	std::string renderType = renderConfig->getType();
	PRT_DEBUG("OK\n  Opening display... ");
	if (renderType == "SDLLo") {
		renderer = createSDLLoRenderer(this, fullScreen, time);
	}
	else if (renderType == "SDLHi") {
		renderer = createSDLHiRenderer(this, fullScreen, time);
	}
	else {
		PRT_ERROR("Unknown renderer \"" << renderType << "\"");
	}

	// Register hotkey for fullscreen togling
	HotKey::instance()->registerAsyncHotKey(SDLK_PRINT, this);

	MSXMotherBoard::instance()->register_IO_In((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_In((byte)0x99, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x99, this);
	if (version == V9938 || version == V9958) {
		MSXMotherBoard::instance()->register_IO_Out((byte)0x9A, this);
		MSXMotherBoard::instance()->register_IO_Out((byte)0x9B, this);
	}

	// First interrupt in Pal mode here
	Scheduler::instance()->setSyncPoint(currentTime+71285, *this); // PAL
}

VDP::~VDP()
{
	PRT_DEBUG("Destroying a VDP object");
	delete(renderer);
	delete[](vramData);
}

void VDP::reset(const EmuTime &time)
{
	static const byte INIT_STATUS_REGS[] = {
		0x00, 0x00, 0x6C, 0x00, 0xFE, 0x00, 0xFC, 0x00,
		0x00, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
		};

	MSXDevice::reset(time);

	currentTime = time;

	for (int i = 0; i < 64; i++) controlRegs[i] = 0;
	displayMode = 0;
	vramPointer = 0;
	readAhead = 0;
	firstByte = -1;

	// Init status registers.
	memcpy(statusRegs, INIT_STATUS_REGS, 16);
	if (version == V9958) statusRegs[1] |= 0x04;

	nameBase = ~(-1 << 10);
	colourBase = ~(-1 << 6);
	patternBase = ~(-1 << 11);
	spriteAttributeBase = 0;
	spritePatternBase = 0;
	spriteAttributeBasePtr = vramData;
	spritePatternBasePtr = vramData;

	// TODO: Reset the renderer.
}

void VDP::signalHotKey(SDLKey key)
{
	// Only key currently registered is full screen toggle.
	fullScreen = !fullScreen;
	renderer->setFullScreen(fullScreen);
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
void VDP::executeUntilEmuTime(const EmuTime &time)
{
	PRT_DEBUG("Executing VDP at time " << time);

	// Update sprite buffer.
	for (int line = 0; line < 192; line++) {
		spriteCount[line] = checkSprites(line, spriteBuffer[line]);
	}

	// This frame is finished.
	// TODO: Actually, a frame ends on vsync, while interrupt
	//   occurs at bottom border start.
	renderer->putImage();

	// Next SP/interrupt in Pal mode here.
	currentTime = time;
	Scheduler::instance()->setSyncPoint(currentTime+71258, *this); //71285 for PAL, 59404 for NTSC
	// Since this is the vertical refresh.
	statusRegs[0] |= 0x80;
	// Set interrupt if bits enable it.
	if (controlRegs[1] & 0x20) {
		setInterrupt();
	}
}

// The I/O functions.

void VDP::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port){
	case 0x98: {
		//fprintf(stderr, "VRAM[%04X]=%02X\n", vramPointer, value);
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
				changeRegister(
					value & controlRegMask,
					firstByte,
					time
					);
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
	case 0x9A:
		fprintf(stderr, "VDP colour write: %02X\n", value);
		break;
	case 0x9B:
		fprintf(stderr, "VDP indirect register write: %02X\n", value);
		break;
	default:
		assert(false);
	}
}

byte VDP::vramRead()
{
	byte ret = readAhead;
	readAhead = vramData[vramPointer];
	vramPointer = (vramPointer + 1) & vramMask;
	firstByte = -1;
	return ret;
}

byte VDP::readIO(byte port, const EmuTime &time)
{
	switch (port) {
	case 0x98:
		return vramRead();
	case 0x99: {
		int activeStatusReg = controlRegs[15];
		byte ret = statusRegs[activeStatusReg];
		switch (activeStatusReg) {
		case 0:
			// TODO: Used to be 0x5F, but that is contradicted by TMS9918.pdf
			statusRegs[0] &= 0x1F;
			resetInterrupt();
			break;
		case 2:
			// TODO: Remove once command engine is added.
			statusRegs[2] |= 0x80;
			// TODO: Remove once accurate timing is in place.
			if ((statusRegs[2] & 0x20) == 0) {
				statusRegs[2] |= 0x20;
			}
			else {
				statusRegs[2] = (statusRegs[2] & ~0x20) ^ 0x40;
			}
			break;
		}
		firstByte = -1;
		//fprintf(stderr, "VDP status reg %d read: %02X\n", activeStatusReg, ret);
		return ret;
	}
	default:
		assert(false);
		return 0xFF;	// prevent warning
	}
}

void VDP::changeRegister(byte reg, byte val, const EmuTime &time)
{
	//fprintf(stderr, "VDP[%02X]=%02X\n", reg, val);

	// TODO: Mask for 00..07 are for MSX1, on MSX2 the masks are
	//   less restrictive.
	static const byte REG_MASKS[64] = {
		0x03, 0xFB, 0x0F, 0xFF, 0x07, 0x7F, 0x07, 0xFF, // 00..07
		0xFB, 0xBF, 0x07, 0x03, 0xFF, 0xFF, 0x07, 0x0F, // 08..15
		0x0F, 0xBF, 0xFF, 0xFF, 0x3F, 0x3F, 0x3F, 0xFF, // 16..23
		0,    0x7F, 0x3F, 0x07, 0,    0,    0,    0,    // 24..31
		0xFF, 0x01, 0xFF, 0x03, 0xFF, 0x01, 0xFF, 0x03, // 32..39
		0xFF, 0x01, 0xFF, 0x03, 0xFF, 0x7F, 0xFF        // 40..46
		};

	val &= REG_MASKS[reg];
	byte oldval = controlRegs[reg];
	if (oldval == val) return;
	controlRegs[reg] = val;

	PRT_DEBUG("VDP: Reg " << (int)reg << " = " << (int)val);
	switch (reg) {
	case 0:
		if ((val ^ oldval) & 2) {
			updateDisplayMode(time);
		}
		break;
	case 1:
		if ((val & 0x20) && (statusRegs[0] & 0x80)) {
			setInterrupt();
		}
		if ((val ^ oldval) & 0x18) {
			updateDisplayMode(time);
		}
		if ((val ^ oldval) & 0x40) {
			renderer->updateDisplayEnabled(time);
		}
		break;
	case 2:
		nameBase = ((val << 10) | ~(-1 << 10)) & vramMask;
		renderer->updateNameBase(time);
		break;
	case 3:
		colourBase = ((val << 6) | ~(-1 << 6)) & vramMask;
		renderer->updateColourBase(time);
		break;
	case 4:
		patternBase = ((val << 11) | ~(-1 << 11)) & vramMask;
		renderer->updatePatternBase(time);
		break;
	case 5: {
		spriteAttributeBase = (val << 7) & vramMask;
		spriteAttributeBasePtr = vramData + spriteAttributeBase;
		renderer->updateSpriteAttributeBase(time);
		break;
	}
	case 6: {
		spritePatternBase = (val << 11) & vramMask;
		spritePatternBasePtr = vramData + spritePatternBase;
		renderer->updateSpritePatternBase(time);
		break;
	}
	case 7:
		if ((val ^ oldval) & 0xF0) {
			renderer->updateForegroundColour(time);
		}
		if ((val ^ oldval) & 0x0F) {
			renderer->updateBackgroundColour(time);
		}
		break;
	}
}

void VDP::updateDisplayMode(const EmuTime &time)
{
	static const char *MODE_STRINGS[] = {
		"Mode 0 (GRAPHIC 1)", "Mode 1 (TEXT 1)", "Mode 2 (GRAPHIC 2)",
		"Mode 1+2 (TEXT 1 variation)", "Mode 3 (MULTICOLOR)",
		"Mode 1+3 (BOGUS)", "Mode 2+3 (MULTICOLOR variation)",
		"Mode 1+2+3 (BOGUS)" };

	int newMode =
		   (controlRegs[0] & 0x02)
		| ((controlRegs[1] & 0x10) >> 4)
		| ((controlRegs[1] & 0x08) >> 1);
	if (newMode != displayMode) {
		displayMode = newMode;
		renderer->updateDisplayMode(time);
		PRT_DEBUG("VDP: mode " << MODE_STRINGS[newMode]);
	}
}

int VDP::doublePattern(int a)
{
	// bit-pattern "abcd" gets expanded to "aabbccdd"
	a =  (a<<16)             |  a;
	a = ((a<< 8)&0x00ffff00) | (a&0xff0000ff);
	a = ((a<< 4)&0x0ff00ff0) | (a&0xf00ff00f);
	a = ((a<< 2)&0x3c3c3c3c) | (a&0xc3c3c3c3);
	a = ((a<< 1)&0x66666666) | (a&0x99999999);
	return a;
}

int VDP::checkSprites(int line, VDP::SpriteInfo *visibleSprites)
{
	if (!spritesEnabled()) return 0;

	// Compensate for the fact that sprites are calculated one line
	// before they are plotted.
	line--;

	// Get sprites for this line and detect 5th sprite if any.
	int sprite, visibleIndex = 0;
	int size = getSpriteSize();
	int magSize = size * (getSpriteMag() + 1);
	int minStart = line - magSize;
	byte *attributePtr = spriteAttributeBasePtr;
	for (sprite = 0; sprite < 32; sprite++, attributePtr += 4) {
		int y = *attributePtr;
		if (y == 208) break;
		if (y > 208) y -= 256;
		if ((y > minStart) && (y <= line)) {
			if (visibleIndex == 4) {
				// Five sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero.
				if (~statusRegs[0] & 0xC0) {
					statusRegs[0] = (statusRegs[0] & 0xE0) | 0x40 | sprite;
				}
				if (limitSprites) break;
			}
			SpriteInfo *sip = &visibleSprites[visibleIndex++];
			int patternIndex = ((size == 16)
				? attributePtr[2] & 0xFC : attributePtr[2]);
			sip->pattern = calculatePattern(patternIndex, line - y);
			sip->x = attributePtr[1];
			if (attributePtr[3] & 0x80) sip->x -= 32;
			sip->colour = attributePtr[3] & 0x0F;
		}
	}
	if (~statusRegs[0] & 0x40) {
		// No 5th sprite detected, store number of latest sprite processed.
		statusRegs[0] = (statusRegs[0] & 0xE0) | (sprite < 32 ? sprite : 31);
	}

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The visibleSprites array is filled now, so we can bail out.
	if (statusRegs[0] & 0x20) return visibleIndex;

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
		int x_i = visibleSprites[i].x;
		int pattern_i = visibleSprites[i].pattern;
		for (int j = i; --j >= 0; ) {
			// Do sprite i and sprite j collide?
			int x_j = visibleSprites[j].x;
			int dist = x_j - x_i;
			if ((-magSize < dist) && (dist < magSize)) {
				int pattern_j = visibleSprites[j].pattern;
				if (dist < 0) {
					pattern_j <<= -dist;
				}
				else if (dist > 0) {
					pattern_j >>= dist;
				}
				if (pattern_i & pattern_j) {
					// Collision!
					statusRegs[0] |= 0x20;
					return visibleIndex;
				}
			}
		}
	}

	return visibleIndex;
}

