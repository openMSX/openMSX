// $Id$

/*
TODO:
- Implement PAL/NTSC timing.
  Note: TMS9928/29 timings are almost the same, but not 100%.
- Apply line-based scheduling.
- Sprite attribute readout probably happens one line in advance.
  This matters when line-based scheduling is operational.
- Get rid of hardcoded port 0x98..0x9B.
*/


#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "SDLLoRenderer.hh"
#include "SDLHiRenderer.hh"
#include "VDP.hh"

#include <SDL/SDL.h>

#include <string>
#include <cassert>


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

	// Set up control register availability.
	static const byte VALUE_MASKS_MSX1[64] = {
		0x03, 0xFB, 0x0F, 0xFF, 0x07, 0x7F, 0x07, 0xFF  // 00..07
		};
	static const byte VALUE_MASKS_MSX2[64] = {
		0x7E, 0x7B, 0x7F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, // 00..07
		0xFB, 0xBF, 0x07, 0x03, 0xFF, 0xFF, 0x07, 0x0F, // 08..15
		0x0F, 0xBF, 0xFF, 0xFF, 0x3F, 0x3F, 0x3F, 0xFF, // 16..23
		0,    0,    0,    0,    0,    0,    0,    0,    // 24..31
		0xFF, 0x01, 0xFF, 0x03, 0xFF, 0x01, 0xFF, 0x03, // 32..39
		0xFF, 0x01, 0xFF, 0x03, 0xFF, 0x7F, 0xFF        // 40..46
		};
	controlRegMask = (isMSX1VDP() ? 0x07 : 0x3F);
	memcpy(controlValueMasks,
		isMSX1VDP() ? VALUE_MASKS_MSX1 : VALUE_MASKS_MSX2, 64);
	if (version == V9958) {
		controlValueMasks[25] = 0x7F;
		controlValueMasks[26] = 0x3F;
		controlValueMasks[27] = 0x07;
	}

	// Video RAM.
	int vramSize =
		(isMSX1VDP() ? 16 : deviceConfig->getParameterAsInt("vram"));
	if (vramSize != 16 && vramSize != 64 && vramSize != 128) {
		PRT_ERROR("VRAM size of " << vramSize << "kB is not supported!");
	}
	vramSize *= 1024;
	vramMask = vramSize - 1;
	vramData = new byte[vramSize];
	// TODO: Use exception instead?
	if (!vramData) return ;//1;
	memset(vramData, 0, vramSize);

	// Put VDP into reset state, but do not call Renderer methods.
	resetInit(time);

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
	if (!isMSX1VDP()) {
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

void VDP::resetInit(const EmuTime &time)
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
	paletteLatch = -1;
	blinkState = false;

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

	// From appendix 8 of the V9938 data book (page 148).
	const word V9938_PALETTE[16] = {
		0x000, 0x000, 0x611, 0x733, 0x117, 0x327, 0x151, 0x627,
		0x171, 0x373, 0x661, 0x664, 0x411, 0x265, 0x555, 0x777
	};
	// Init the palette.
	memcpy(palette, V9938_PALETTE, 16 * sizeof(word));
}

void VDP::reset(const EmuTime &time)
{
	resetInit(time);
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
		int addr = ((controlRegs[14] << 14) | vramPointer) & vramMask;
		//fprintf(stderr, "VRAM[%05X]=%02X\n", addr, value);
		setVRAM(addr, value, time);
		vramPointer = (vramPointer + 1) & 0x3FFF;
		if (vramPointer == 0 && (displayMode & 0x18)) {
			// In MSX2 video modes, pointer range is 128K.
			controlRegs[14] = (controlRegs[14] + 1) & 0x07;
		}
		readAhead = value;
		firstByte = -1;
		break;
	}
	case 0x99:
		if (firstByte == -1) {
			firstByte = value;
		}
		else {
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
				vramPointer = ((word)value << 8 | firstByte) & 0x3FFF;
				if (!(value & 0x40)) {
					// Read ahead.
					vramRead();
				}
			}
			firstByte = -1;
		}
		break;
	case 0x9A:
		if (paletteLatch == -1) {
			paletteLatch = value;
		}
		else {
			int index = controlRegs[16];
			palette[index] = paletteLatch | (value << 8);
			renderer->updatePalette(index, time);
			controlRegs[16] = (index + 1) & 0x0F;
			paletteLatch = -1;
		}
		break;
	case 0x9B: {
		// TODO: Does port 0x9B affect firstByte?
		// TODO: What happens if reg 17 is written indirectly?
		//fprintf(stderr, "VDP indirect register write: %02X\n", value);
		byte regNr = controlRegs[17];
		changeRegister(regNr & 0x3F, value, time);
		if ((regNr & 0x80) == 0) {
			// Auto-increment.
			controlRegs[17] = (regNr + 1) & 0x3F;
		}
		break;
	}
	default:
		assert(false);
	}
}

byte VDP::vramRead()
{
	byte ret = readAhead;
	readAhead = vramData[
		((controlRegs[14] << 14) | vramPointer) & vramMask ];
	vramPointer = (vramPointer + 1) & 0x3FFF;
	if (vramPointer == 0 && (displayMode & 0x18)) {
		// In MSX2 video mode, pointer range is 128K.
		controlRegs[14] = (controlRegs[14] + 1) & 0x07;
	}
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

	val &= controlValueMasks[reg];
	byte oldval = controlRegs[reg];
	if (oldval == val) return;
	controlRegs[reg] = val;

	PRT_DEBUG("VDP: Reg " << (int)reg << " = " << (int)val);
	switch (reg) {
	case 0:
		if ((val ^ oldval) & 0x0E) {
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
	case 10:
		colourBase = ((controlRegs[10] << 14) | (controlRegs[3] << 6)
			| ~(-1 << 6)) & vramMask;
		renderer->updateColourBase(time);
		break;
	case 4:
		patternBase = ((val << 11) | ~(-1 << 11)) & vramMask;
		renderer->updatePatternBase(time);
		break;
	case 5:
	case 11:
		spriteAttributeBase =
			((controlRegs[11] << 15) | (controlRegs[5] << 7)) & vramMask;
		spriteAttributeBasePtr = vramData + spriteAttributeBase;
		renderer->updateSpriteAttributeBase(time);
		break;
	case 6:
		spritePatternBase = (val << 11) & vramMask;
		spritePatternBasePtr = vramData + spritePatternBase;
		renderer->updateSpritePatternBase(time);
		break;
	case 7:
		if ((val ^ oldval) & 0xF0) {
			renderer->updateForegroundColour(time);
		}
		if ((val ^ oldval) & 0x0F) {
			renderer->updateBackgroundColour(time);
		}
		break;
	case 16:
		// Any half-finished palette loads are aborted.
		paletteLatch = -1;
		break;
	}
}

void VDP::updateDisplayMode(const EmuTime &time)
{
	int newMode =
		  ((controlRegs[0] & 0x0E) << 1)
		| ((controlRegs[1] & 0x08) >> 2)
		| ((controlRegs[1] & 0x10) >> 4);
	if (newMode != displayMode) {
		fprintf(stderr, "VDP: mode %02X\n", newMode);
		displayMode = newMode;
		renderer->updateDisplayMode(time);
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
			int patternIndex = (size == 16
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

