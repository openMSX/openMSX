// $Id$

/*
TODO:
- Run more measurements on real MSX to find out how horizontal
  scanning interrupt really works.
  Finish model and implement it.
  Especially test this scenario:
  * IE1 enabled, interrupt occurs
  * wait until matching line is passed
  * disable IE1
  * read FH
  * read FH
  Current implementation would return FH=0 both times.
- Keep blank / display knowledge in VDP instead of renderer:
  * VDP has all the info required to calculate it
  * VDP has to know it for sprite checking in overscan
  * command engine wants to know as well
- Implement vertical display adjust.
- Check how Z80 should treat interrupts occurring during DI.
- Sprite attribute readout probably happens one line in advance.
  This matters when line-based scheduling is operational.
- Speed up checkSpritesN by administrating which lines contain which
  sprites in a bit vector.
  This avoids cycling through all 32 possible sprites on every line.
  Keeping administration up-to-date is not that hard and happens
  at a low frequency (typically once per frame).
- Verify model for 5th sprite number calculation.
  For example, does it have the right value in text mode?
- Get rid of hardcoded port 0x98..0x9B.
- Implement overscan.
  What is the maximum number of lines? (during overscan)
  Currently fixed to 212, but those tables should be expanded.
  (grep for 212 in *.cc AND *.hh)
*/


#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "SDLLoRenderer.hh"
#include "SDLHiRenderer.hh"
#include "VDP.hh"
#include "VDPCmdEngine.hh"

#include <SDL/SDL.h>
#include <string>
#include <cassert>

// Inlined methods first, to make sure they are actually inlined:

inline int VDP::calculatePattern(int patternNr, int y)
{
	// TODO: Optimise getSpriteSize?
	if (getSpriteMag()) y /= 2;
	int pattern = spritePatternBasePtr[patternNr * 8 + y] << 24;
	if (getSpriteSize() == 16) {
		pattern |= spritePatternBasePtr[patternNr * 8 + y + 16] << 16;
	}
	return (getSpriteMag() ? doublePattern(pattern) : pattern);
}

inline int VDP::checkSprites1(int line, VDP::SpriteInfo *visibleSprites)
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
		// Compensate for vertical scroll.
		// TODO: Use overscan and check what really happens.
		y = (y - getVerticalScroll()) & 0xFF;
		if (y > 208) y -= 256;
		if ((y > minStart) && (y <= line)) {
			if (visibleIndex == 4) {
				// Five sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero.
				if (~statusReg0 & 0xC0) {
					statusReg0 = (statusReg0 & 0xE0) | 0x40 | sprite;
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
	if (~statusReg0 & 0x40) {
		// No 5th sprite detected, store number of latest sprite processed.
		statusReg0 = (statusReg0 & 0xE0) | (sprite < 32 ? sprite : 31);
	}

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The visibleSprites array is filled now, so we can bail out.
	if (statusReg0 & 0x20) return visibleIndex;

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
					statusReg0 |= 0x20;
					// TODO: Fill in collision coordinates in S#3..S#6.
					// ...Unless this feature only works in sprite mode 2.
					return visibleIndex;
				}
			}
		}
	}

	return visibleIndex;
}

inline int VDP::checkSprites2(int line, VDP::SpriteInfo *visibleSprites)
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
	// TODO: Verify exact address calculation and maybe masks while
	//       processing the tables.
	byte *colourPtr = vramData + (spriteAttributeBase & 0x1FC00);
	byte *attributePtr = colourPtr + 512;
	// TODO: Implement CC bit (priority cancellation, OR-ing pixels).
	// TODO: Implement IC bit (immunity to collisions).
	for (sprite = 0; sprite < 32; sprite++,
			attributePtr += 4, colourPtr += 16) {
		int y = *attributePtr;
		if (y == 216) break;
		// Compensate for vertical scroll.
		// TODO: Use overscan and check what really happens.
		y = (y - getVerticalScroll()) & 0xFF;
		if (y > 216) y -= 256;
		if ((y > minStart) && (y <= line)) {
			if (visibleIndex == 8) {
				// Nine sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero. Stuck to this for V9938.
				if (~statusReg0 & 0xC0) {
					statusReg0 = (statusReg0 & 0xE0) | 0x40 | sprite;
				}
				if (limitSprites) break;
			}
			SpriteInfo *sip = &visibleSprites[visibleIndex++];
			int patternIndex = (size == 16
				? attributePtr[2] & 0xFC : attributePtr[2]);
			sip->pattern = calculatePattern(patternIndex, line - y);
			sip->x = attributePtr[1];
			int colourAttrib = colourPtr[line - y];
			if (colourAttrib & 0x80) sip->x -= 32;
			sip->colour = colourAttrib & 0x0F;
		}
	}
	if (~statusReg0 & 0x40) {
		// No 5th sprite detected, store number of latest sprite processed.
		statusReg0 = (statusReg0 & 0xE0) | (sprite < 32 ? sprite : 31);
	}

	// Optimisation:
	// If collision already occurred,
	// that state is stable until it is reset by a status reg read,
	// so no need to execute the checks.
	// The visibleSprites array is filled now, so we can bail out.
	if (statusReg0 & 0x20) return visibleIndex;

	/*
	Model for sprite collision: (or "coincidence" in TMS9918 data sheet)
	Reset when status reg is read.
	Set when sprite patterns overlap.
	Colour doesn't matter: sprites of colour 0 can collide.
	  TODO: V9938 data book denies this (page 98).
	Sprites with off-screen position can collide.

	Implemented by checking every pair for collisions.
	For large numbers of sprites that would be slow.
	There are max 8 sprites and therefore max 42 pairs.
	  TODO: Maybe this is slow... Think of something faster.
	        Probably new approach is needed anyway for OR-ing.
	If any collision is found, method returns at once.
	*/
	for (int i = (visibleIndex < 8 ? visibleIndex : 8); --i >= 1; ) {
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
					statusReg0 |= 0x20;
					// TODO: Fill in collision coordinates in S#3..S#6.
					//       See page 97 for info.
					return visibleIndex;
				}
			}
		}
	}

	return visibleIndex;
}

inline void VDP::updateIRQ()
{
	// TODO: Motherboard implementation can handle multiple
	//       IRQs from a single device, but its documentation
	//       says it cannot.
	//       Changing that would allow this method to be replaced
	//       by raiseIRQ() and lowerIRQ() calls from horizontal and
	//       vertical scanning interrupt code.
	if (irqVertical || irqHorizontal) {
		setInterrupt();
	} else {
		resetInterrupt();
	}
}

// Init and cleanup:

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
	static const byte VALUE_MASKS_MSX1[32] = {
		0x03, 0xFB, 0x0F, 0xFF, 0x07, 0x7F, 0x07, 0xFF  // 00..07
		};
	static const byte VALUE_MASKS_MSX2[32] = {
		0x7E, 0x7B, 0x7F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, // 00..07
		0xFB, 0xBF, 0x07, 0x03, 0xFF, 0xFF, 0x07, 0x0F, // 08..15
		0x0F, 0xBF, 0xFF, 0xFF, 0x3F, 0x3F, 0x3F, 0xFF, // 16..23
		0,    0,    0,    0,    0,    0,    0,    0,    // 24..31
		};
	controlRegMask = (isMSX1VDP() ? 0x07 : 0x3F);
	memcpy(controlValueMasks,
		isMSX1VDP() ? VALUE_MASKS_MSX1 : VALUE_MASKS_MSX2, 32);
	if (version == V9958) {
		// Enable V9958-specific control registers.
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

	// Create command engine.
	cmdEngine = new VDPCmdEngine(this, time);

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

	// Init scheduling.
	hScanSyncTime = 0;
	frameStart(time);
}

VDP::~VDP()
{
	PRT_DEBUG("Destroying a VDP object");
	delete(renderer);
	delete[](vramData);
}

void VDP::resetInit(const EmuTime &time)
{
	MSXDevice::reset(time);

	for (int i = 0; i < 32; i++) controlRegs[i] = 0;
	if (version == TMS9929A) {
		// Boots (and remains) in PAL mode, all other VDPs boot in NTSC.
		controlRegs[9] |= 0x02;
	}
	displayMode = 0;
	vramPointer = 0;
	readAhead = 0;
	firstByte = -1;
	paletteLatch = -1;
	blinkState = false;

	// Init status registers.
	statusReg0 = 0x00;
	statusReg1 = (version == V9958 ? 0x04 : 0x00);
	statusReg2 = 0x0C;
	collisionX = 0;
	collisionY = 0;

	// Update IRQ to reflect new register values.
	irqVertical = false;
	irqHorizontal = false;
	updateIRQ();

	nameBase = ~(-1 << 10);
	colourBase = ~(-1 << 6);
	patternBase = ~(-1 << 11);
	spriteAttributeBase = 0;
	spritePatternBase = 0;
	spriteAttributeBasePtr = vramData;
	spritePatternBasePtr = vramData;

	// No sprites found.
	memset(spriteCount, 0, 212 * sizeof(int));

	// From appendix 8 of the V9938 data book (page 148).
	const word V9938_PALETTE[16] = {
		0x000, 0x000, 0x611, 0x733, 0x117, 0x327, 0x151, 0x627,
		0x171, 0x373, 0x661, 0x664, 0x411, 0x265, 0x555, 0x777
	};
	// Init the palette.
	memcpy(palette, V9938_PALETTE, 16 * sizeof(word));

	// TODO: Real VDP probably resets timing as well.
}

void VDP::reset(const EmuTime &time)
{
	resetInit(time);
	// TODO: Reset the renderer.
	// TODO: Reset the command engine.
}

void VDP::signalHotKey(SDLKey key)
{
	// Only key currently registered is full screen toggle.
	fullScreen = !fullScreen;
	renderer->setFullScreen(fullScreen);
}

void VDP::executeUntilEmuTime(const EmuTime &time, int userData)
{
	/*
	PRT_DEBUG("Executing VDP at time " << time
		<< ", sync type " << userData);
	*/
	/*
	int ticksThisFrame = getTicksThisFrame(time);
	cout << (userData == VSYNC ? "VSYNC" :
			(userData == VSCAN ? "VSCAN" : "HSCAN"))
		<< " at (" << (ticksThisFrame % TICKS_PER_LINE)
		<< "," << ((ticksThisFrame - displayStart) / TICKS_PER_LINE)
		<< "), IRQ_H = " << (int)irqHorizontal
		<< " IRQ_V = " << (int)irqVertical
		<< ", frame = " << frameStartTime << "\n";
	*/

	// Handle the various sync types.
	switch (userData) {
	case VSYNC: {
		// Sync with command engine.
		// TODO: This wouldn't be necessary if command engine is synced
		//       on VRAM reads.
		cmdEngine->sync(time);

		// This frame is finished.
		renderer->putImage(time);

		// Begin next frame.
		frameStart(time);
		break;
	}
	case VSCAN:
		// Update sprite buffer.
		// TODO: This is a patch to cover up an inaccuracy.
		//       Most programs change sprite attributes on VSCAN int.
		//       The right way to do it is sync on VRAM writes.
		updateSprites(time);

		// Vertical scanning occurs.
		statusReg0 |= 0x80;
		if (controlRegs[1] & 0x20) {
			irqVertical = true;
			updateIRQ();
		}
		break;
	case HSCAN: {
		// TODO: This implements Marat's guess, what does real V9938 do?
		// TODO: If it is correct, implement it in scheduleHScan instead:
		//       it is faster not the schedule HSCAN in the first place
		//       than to ignore a sync.
		/*
		int ticksThisFrame = getTicksThisFrame(time);
		int displayEnd =
			displayStart + getNumberOfLines() * TICKS_PER_LINE;
		bool vr = ticksThisFrame < displayStart
			|| ticksThisFrame >= displayEnd;
		if (vr) break;
		*/

		// Horizontal scanning occurs.
		if (controlRegs[0] & 0x10) {
			irqHorizontal = true;
			updateIRQ();
		}
		break;
	}
	default:
		assert(false);
	}

}

void VDP::scheduleHScan(const EmuTime &time)
{
	// Remove pending HSCAN sync point, if any.
	if (hScanSyncTime > time) {
		Scheduler::instance()->removeSyncPoint(this, HSCAN);
		hScanSyncTime = time;
	}

	// Calculate moment in time line match occurs.
	horizontalScanOffset = displayStart
		+ ((controlRegs[19] - controlRegs[23]) & 0xFF) * TICKS_PER_LINE
		+ (isTextMode() ?
			TICKS_PER_LINE - 87 - 27 : TICKS_PER_LINE - 59 - 27);

	// Register new HSCAN sync point if interrupt is enabled.
	if (controlRegs[0] & 0x10) {
		hScanSyncTime = frameStartTime + horizontalScanOffset;
		if (hScanSyncTime > time) {
			Scheduler::instance()->setSyncPoint(hScanSyncTime, this, HSCAN);
		}
	}
}

// TODO: inline?
void VDP::frameStart(const EmuTime &time)
{
	//cout << "VDP::frameStart @ " << time << "\n";

	// Toggle E/O.
	// Actually this should occur half a line earlier,
	// but for now this is accurate enough.
	statusReg2 ^= 0x02;

	// Settings which are fixed at start of frame.
	// Not sure this is how real MSX does it, but close enough for now.
	palTiming = controlRegs[9] & 0x02;
	verticalAdjust = (controlRegs[18] >> 4) ^ 0x07;

	spriteLine = 0;
	frameStartTime = time;
	// TODO: Use display adjust register (R#18).
	displayStart =
		( palTiming
		? ( controlRegs[9] & 0x80
		  ? (3 + 13 + 43) * TICKS_PER_LINE
		  : (3 + 13 + 53) * TICKS_PER_LINE
		  )
		: ( controlRegs[9] & 0x80
		  ? (3 + 13 + 16) * TICKS_PER_LINE
		  : (3 + 13 + 26) * TICKS_PER_LINE
		  )
		);
	//cout << "VDP::frameStart PASS 1\n";
	scheduleHScan(time);
	//cout << "VDP::frameStart PASS 2\n";
	Scheduler::instance()->setSyncPoint(
		frameStartTime + (displayStart +
			( controlRegs[9] & 0x80
			? 212 * TICKS_PER_LINE
			: 192 * TICKS_PER_LINE
			)
		), this, VSCAN);
	//cout << "VDP::frameStart PASS 3\n";
	Scheduler::instance()->setSyncPoint(
		frameStartTime + getTicksPerFrame(),
		this, VSYNC);
	/*
	cout << "--> frameStart = " << frameStartTime
		<< ", frameEnd = " << (frameStartTime + getTicksPerFrame())
		<< ", hscan = " << hScanSyncTime
		<< ", displayStart = " << displayStart
		<< ", timing: " << (palTiming ? "PAL" : "NTSC")
		<< "\n";
	*/
}

void VDP::updateSprites1(int limit)
{
	// TODO: Replace by display / blank check.
	if (limit > 212) limit = 212;
	for ( ; spriteLine < limit; spriteLine++) {
		spriteCount[spriteLine] =
			checkSprites1(spriteLine, spriteBuffer[spriteLine]);
	}
}

void VDP::updateSprites2(int limit)
{
	// TODO: Replace by display / blank check.
	if (limit > 212) limit = 212;
	for ( ; spriteLine < limit; spriteLine++) {
		spriteCount[spriteLine] =
			checkSprites2(spriteLine, spriteBuffer[spriteLine]);
	}
}

// The I/O functions.

void VDP::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port){
	case 0x98: {
		int addr = ((controlRegs[14] << 14) | vramPointer) & vramMask;
		//fprintf(stderr, "VRAM[%05X]=%02X\n", addr, value);
		// TODO: Check MXC bit (R#45, bit 6) for extension RAM access.
		//       This bit is kept by the command engine.

		// First sync with the command engine, which can write VRAM.
		cmdEngine->updateVRAM(addr, time);
		// Then sync sprite checking, which only reads VRAM and
		// is used by the Renderer.
		updateSprites(time);
		// Then sync with the Renderer.
		setVRAM(addr, value, time);

		// Finally, commit the change.
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
			int grb = paletteLatch | (value << 8);
			renderer->updatePalette(index, grb, time);
			palette[index] = grb;
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
	assert(port == 0x98 || port == 0x99);
	if (port == 0x98) {
		return vramRead();
	}
	else { // port == 0x99

		// Abort any port 0x99 writes in progress.
		firstByte = -1;

		//cout << "read S#" << (int)controlRegs[15] << "\n";

		// Calculate status register contents.
		switch (controlRegs[15]) {
		case 0: {
			byte ret = statusReg0;
			// TODO: Used to be 0x5F, but that is contradicted by
			//       TMS9918.pdf. Check on real MSX.
			statusReg0 &= 0x1F;
			irqVertical = false;
			updateIRQ();
			return ret;
		}
		case 1: {
			/*
			int ticksThisFrame = getTicksThisFrame(time);
			cout << "S#1 read at (" << (ticksThisFrame % TICKS_PER_LINE)
				<< "," << ((ticksThisFrame - displayStart) / TICKS_PER_LINE)
				<< "), IRQ_H = " << (int)irqHorizontal
				<< " IRQ_V = " << (int)irqVertical
				<< ", frame = " << frameStartTime << "\n";
			*/

			if (controlRegs[0] & 0x10) { // line int enabled
				byte ret = statusReg1 | irqHorizontal;
				irqHorizontal = false;
				updateIRQ();
				return ret;
			} else { // line int disabled
				// FH goes up at the start of the right border of IL and
				// goes down at the start of the next left border.
				// TODO: Precalc matchLength?
				int afterMatch =
					getTicksThisFrame(time) - horizontalScanOffset;
				int matchLength = isTextMode()
					? 87 + 27 + 100 + 102 : 59 + 27 + 100 + 102;
				return statusReg1
					| (0 <= afterMatch && afterMatch < matchLength);
			}
		}
		case 2: {
			int ticksThisFrame = getTicksThisFrame(time);

			// TODO: Once VDP keeps display/blanking state, keeping
			//       VR is probably part of that, so use it.
			int displayEnd =
				displayStart + getNumberOfLines() * TICKS_PER_LINE;
			bool vr = ticksThisFrame < displayStart
			       || ticksThisFrame >= displayEnd;

			return statusReg2
				| (getHR(ticksThisFrame) ? 0x20 : 0x00)
				| (vr ? 0x40 : 0x00)
				| cmdEngine->getStatus(time);
		}
		case 3:
			return (byte)collisionX;
		case 4:
			return (byte)(collisionX >> 8) | 0xFE;
		case 5: {
			byte ret = (byte)collisionY;
			collisionX = collisionY = 0;
			return ret;
		}
		case 6:
			return (byte)(collisionY >> 8) | 0xFC;
		case 7:
			return cmdEngine->readColour(time);
		case 8:
			return (byte)cmdEngine->getBorderX(time);
		case 9:
			return (byte)(cmdEngine->getBorderX(time) >> 8) | 0xFE;
		default: // non-existent status register
			return 0xFF;
		}
	}
}

void VDP::changeRegister(byte reg, byte val, const EmuTime &time)
{
	//fprintf(stderr, "VDP[%02X]=%02X\n", reg, val);

	// Pass command register writes to command engine.
	if (32 <= reg && reg < 47) {
		cmdEngine->setCmdReg(reg - 32, val, time);
		return;
	}

	val &= controlValueMasks[reg];
	byte oldval = controlRegs[reg];
	byte change = val ^ oldval;
	if (!change) return;
	//PRT_DEBUG("VDP: Reg " << (int)reg << " = " << (int)val);

	// Perform additional tasks before new value becomes active.
	switch (reg) {
	case 0:
		if ((val ^ oldval) & 0x0E) {
			updateSprites(time);
			updateDisplayMode(val, controlRegs[1], time);
		}
		break;
	case 1:
		if (change & 0x5B) {
			// Update sprites on blank, mode, size and mag changes.
			updateSprites(time);
		}
		// TODO: Reset irqVertical if IE0 is reset?
		if (change & 0x18) {
			updateDisplayMode(controlRegs[0], val, time);
		}
		if (change & 0x40) {
			renderer->updateDisplayEnabled(!isDisplayEnabled(), time);
		}
		break;
	case 2: {
		int addr = ((val << 10) | ~(-1 << 10)) & vramMask;
		renderer->updateNameBase(addr, time);
		nameBase = addr;
		break;
	}
	case 3: {
		int addr = ((controlRegs[10] << 14) | (val << 6)
			| ~(-1 << 6)) & vramMask;
		renderer->updateColourBase(addr, time);
		colourBase = addr;
		break;
	}
	case 10: {
		int addr = ((val << 14) | (controlRegs[3] << 6)
			| ~(-1 << 6)) & vramMask;
		renderer->updateColourBase(addr, time);
		colourBase = addr;
		break;
	}
	case 4: {
		int addr = ((val << 11) | ~(-1 << 11)) & vramMask;
		renderer->updatePatternBase(addr, time);
		patternBase = addr;
		break;
	}
	case 5: {
		updateSprites(time);
		int addr = ((controlRegs[11] << 15) | (val << 7)) & vramMask;
		renderer->updateSpriteAttributeBase(addr, time);
		spriteAttributeBase = addr;
		spriteAttributeBasePtr = vramData + spriteAttributeBase;
		break;
	}
	case 11: {
		updateSprites(time);
		int addr = ((val << 15) | (controlRegs[5] << 7)) & vramMask;
		renderer->updateSpriteAttributeBase(addr, time);
		spriteAttributeBase = addr;
		spriteAttributeBasePtr = vramData + spriteAttributeBase;
		break;
	}
	case 6: {
		updateSprites(time);
		int addr = (val << 11) & vramMask;
		renderer->updateSpritePatternBase(addr, time);
		spritePatternBase = addr;
		spritePatternBasePtr = vramData + spritePatternBase;
		break;
	}
	case 7:
		if ((val ^ oldval) & 0xF0) {
			renderer->updateForegroundColour(val >> 4, time);
		}
		if ((val ^ oldval) & 0x0F) {
			renderer->updateBackgroundColour(val & 0x0F, time);
		}
		break;
	case 16:
		// Any half-finished palette loads are aborted.
		paletteLatch = -1;
		break;
	case 18:
		if ((val ^ oldval) & 0x0F) {
			renderer->updateHorizontalAdjust((val & 0x0F) ^ 0x07, time);
		}
		break;
	case 23:
		updateSprites(time);
		renderer->updateVerticalScroll(val, time);
		break;
	}

	// Commit the change.
	controlRegs[reg] = val;

	// Perform additional tasks after new value became active.
	switch (reg) {
	case 0:
		if (change & 0x10) { // IE1
			if (val & 0x10) {
				scheduleHScan(time);
			} else {
				irqHorizontal = false;
				updateIRQ();
			}
		}
		break;
	case 1:
		if (change & 0x20) { // IE0
			if (!(val & 0x20)) {
				irqVertical = false;
				updateIRQ();
			}
		}
		break;
	case 19:
	case 23:
		scheduleHScan(time);
		break;
	}
}

void VDP::updateDisplayMode(byte reg0, byte reg1, const EmuTime &time)
{
	int newMode =
		  ((reg0 & 0x0E) << 1)  // M5..M3
		| ((reg1 & 0x08) >> 2)  // M2
		| ((reg1 & 0x10) >> 4); // M1
	if (newMode != displayMode) {
		//PRT_DEBUG("VDP: mode " << newMode);
		renderer->updateDisplayMode(newMode, time);
		cmdEngine->updateDisplayMode(newMode, time);
		displayMode = newMode;
		// To be extremely accurate, reschedule hscan when changing
		// from/to text mode. Text mode has different border width,
		// which affects the moment hscan occurs.
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

