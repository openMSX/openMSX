// $Id$

/*
TODO:
- Put VRAM in a separate class?
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
- Verify for which lines sprite checking occurs.
  It should happen for all display lines.
  Pitfalls:
  * on overscan, there are many more display lines
  * make sure that renderer behaviour does not affect the outcome
    so: independent of *which* lines are requested and *when* they are
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
*/

#include "MSXMotherBoard.hh"
#include "VDP.hh"
#include "VDPCmdEngine.hh"
#include "PlatformFactory.hh"
#include "ConsoleSource/Console.hh"
#include "ConsoleSource/CommandController.hh"

#include <string>
#include <cassert>

// Inlined methods first, to make sure they are actually inlined:

// TODO: Separate planar / non-planar routines.
inline VDP::SpritePattern VDP::calculatePattern(int patternNr, int y)
{
	// TODO: Optimise getSpriteSize?
	if (getSpriteMag()) y /= 2;
	SpritePattern pattern = getVRAMReordered(
		spritePatternBase + patternNr * 8 + y) << 24;
	if (getSpriteSize() == 16) {
		pattern |= getVRAMReordered(
			spritePatternBase + patternNr * 8 + y + 16) << 16;
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
	byte *attributePtr = vramData + spriteAttributeBase;
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
			sip->colourAttrib = attributePtr[3];
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
		SpritePattern pattern_i = visibleSprites[i].pattern;
		for (int j = i; --j >= 0; ) {
			// Do sprite i and sprite j collide?
			int x_j = visibleSprites[j].x;
			int dist = x_j - x_i;
			if ((-magSize < dist) && (dist < magSize)) {
				SpritePattern pattern_j = visibleSprites[j].pattern;
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

// TODO: For higher performance, have separate routines for planar and
//       non-planar modes.
inline int VDP::checkSprites2(int line, VDP::SpriteInfo *visibleSprites)
{
	if (!spritesEnabled()) return 0;

	// Compensate for the fact that sprites are calculated one line
	// before they are plotted.
	// TODO: Actually fetch the data one line earlier.
	line--;

	// Get sprites for this line and detect 5th sprite if any.
	int sprite, visibleIndex = 0;
	int size = getSpriteSize();
	int magSize = size * (getSpriteMag() + 1);
	int minStart = line - magSize;
	// TODO: Should masks be applied while processing the tables?
	int colourAddr = spriteAttributeBase & 0x1FC00;
	int attributeAddr = colourAddr + 512;
	// TODO: Verify CC implementation.
	for (sprite = 0; sprite < 32; sprite++,
			attributeAddr += 4, colourAddr += 16) {
		int y = getVRAMReordered(attributeAddr);
		if (y == 216) break;
		// Compensate for vertical scroll.
		// TODO: Use overscan and check what really happens.
		y = (y - getVerticalScroll()) & 0xFF;
		if (y > 216) y -= 256;
		if (minStart < y && y <= line) {
			if (visibleIndex == 8) {
				// Nine sprites on a line.
				// According to TMS9918.pdf 5th sprite detection is only
				// active when F flag is zero. Stuck to this for V9938.
				if (~statusReg0 & 0xC0) {
					statusReg0 = (statusReg0 & 0xE0) | 0x40 | sprite;
				}
				if (limitSprites) break;
			}
			byte colourAttrib = getVRAMReordered(colourAddr + line - y);
			// Sprites with CC=1 are only visible if preceded by
			// a sprite with CC=0.
			if ((colourAttrib & 0x40) && visibleIndex == 0) continue;
			SpriteInfo *sip = &visibleSprites[visibleIndex++];
			int patternIndex = getVRAMReordered(attributeAddr + 2);
			// TODO: Precalc pattern index mask.
			if (size == 16) patternIndex &= 0xFC;
			sip->pattern = calculatePattern(patternIndex, line - y);
			sip->x = getVRAMReordered(attributeAddr + 1);
			if (colourAttrib & 0x80) sip->x -= 32;
			sip->colourAttrib = colourAttrib;
		}
	}
	if (~statusReg0 & 0x40) {
		// No 9th sprite detected, store number of latest sprite processed.
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
		// If CC or IC is set, this sprite cannot collide.
		if (visibleSprites[i].colourAttrib & 0x60) continue;

		int x_i = visibleSprites[i].x;
		SpritePattern pattern_i = visibleSprites[i].pattern;
		for (int j = i; --j >= 0; ) {
			// If CC or IC is set, this sprite cannot collide.
			if (visibleSprites[j].colourAttrib & 0x60) continue;

			// Do sprite i and sprite j collide?
			int x_j = visibleSprites[j].x;
			int dist = x_j - x_i;
			if ((-magSize < dist) && (dist < magSize)) {
				SpritePattern pattern_j = visibleSprites[j].pattern;
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
					// TODO: I guess the VDP checks for collisions while
					//       scanning, if so the top-leftmost collision
					//       should be remembered. Currently the topmost
					//       line is guaranteed, but within that line
					//       the highest sprite numbers are selected.
					return visibleIndex;
				}
			}
		}
	}

	return visibleIndex;
}

// Init and cleanup:

VDP::VDP(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
	, paletteCmd(this)
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

	// Create renderer.
	renderer = PlatformFactory::createRenderer(this, time);

	MSXMotherBoard::instance()->register_IO_In((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_In((byte)0x99, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x99, this);
	if (!isMSX1VDP()) {
		MSXMotherBoard::instance()->register_IO_Out((byte)0x9A, this);
		MSXMotherBoard::instance()->register_IO_Out((byte)0x9B, this);
	}

	// Init scheduling.
	displayStartSyncTime = 0;
	vScanSyncTime = 0;
	hScanSyncTime = 0;
	frameStart(time);

	// Register console commands.
	CommandController::instance()->registerCommand(paletteCmd, "palette");

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
	blinkCount = 0;

	// Init status registers.
	statusReg0 = 0x00;
	statusReg1 = (version == V9958 ? 0x04 : 0x00);
	statusReg2 = 0x0C;
	collisionX = 0;
	collisionY = 0;

	// Update IRQ to reflect new register values.
	irqVertical.reset();
	irqHorizontal.reset();

	nameBase = ~(-1 << 10);
	colourBase = ~(-1 << 6);
	patternBase = ~(-1 << 11);
	spriteAttributeBase = 0;
	spritePatternBase = 0;

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
	isDisplayArea = false;
}

void VDP::reset(const EmuTime &time)
{
	resetInit(time);
	// TODO: Reset the renderer.
	// TODO: Reset the command engine.
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
		<< "), IRQ_H = " << (int)irqHorizontal.getState()
		<< " IRQ_V = " << (int)irqVertical.getState()
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
	case DISPLAY_START:
		// Display area starts here, unless we're doing overscan and it
		// was already active.
		if (!isDisplayArea) {
			if (controlRegs[1] & 0x40) {
				renderer->updateDisplayEnabled(true, time);
			}
			isDisplayArea = true;
		}
		break;
	case VSCAN:
		// Update sprite buffer.
		// TODO: This is a patch to cover up an inaccuracy.
		//       Most programs change sprite attributes on VSCAN int.
		//       If a Renderer calls getSprites, the sprite buffer is
		//       updated as well, but the VDP should not rely on Renderer
		//       behaviour.
		//       The right way to do it is sync on VRAM writes.
		updateSprites(time);

		// VSCAN is the end of display.
		if (isDisplayEnabled()) {
			renderer->updateDisplayEnabled(false, time);
		}
		isDisplayArea = false;

		// Vertical scanning occurs.
		statusReg0 |= 0x80;
		if (controlRegs[1] & 0x20) irqVertical.set();
		break;
	case HSCAN:
		// Horizontal scanning occurs.
		if (controlRegs[0] & 0x10) irqHorizontal.set();
		break;
	default:
		assert(false);
	}

}

// TODO: This approach assumes that an overscan-like approach can be used
//       skip display start, so that the border is rendered instead.
//       This makes sense, but it has not been tested on real MSX yet.
void VDP::scheduleDisplayStart(const EmuTime &time)
{
	// Remove pending DISPLAY_START sync point, if any.
	if (displayStartSyncTime > time) {
		Scheduler::instance()->removeSyncPoint(this, DISPLAY_START);
		//cerr << "removing predicted DISPLAY_START sync point\n";
	}

	// Calculate when (lines and time) display starts.
	lineZero =
		( palTiming
		? (controlRegs[9] & 0x80 ? 3 + 13 + 36 : 3 + 13 + 46)
		: (controlRegs[9] & 0x80 ? 3 + 13 +  9 : 3 + 13 + 19)
		) + verticalAdjust;
	displayStart =
		( isDisplayArea // overscan?
		? 3 + 13 // sync + top erase
		: lineZero * TICKS_PER_LINE
		);
	displayStartSyncTime = frameStartTime + displayStart;
	//cerr << "new DISPLAY_START is " << (displayStart / TICKS_PER_LINE) << "\n";

	// Register new DISPLAY_START sync point.
	if (displayStartSyncTime > time) {
		Scheduler::instance()->setSyncPoint(
			displayStartSyncTime, this, DISPLAY_START);
		//cerr << "inserting new DISPLAY_START sync point\n";
	}

	// HSCAN and VSCAN are relative to display start.
	scheduleHScan(time);
	scheduleVScan(time);
}

void VDP::scheduleVScan(const EmuTime &time)
{
	/*
	cerr << "scheduleVScan @ " << (getTicksThisFrame(time) / TICKS_PER_LINE) << "\n";
	if (vScanSyncTime < frameStartTime) {
		cerr << "old VSCAN was previous frame\n";
	} else {
		cerr << "old VSCAN was " << (frameStartTime.getTicksTill(vScanSyncTime) / TICKS_PER_LINE) << "\n";
	}
	*/

	// Remove pending VSCAN sync point, if any.
	if (vScanSyncTime > time) {
		Scheduler::instance()->removeSyncPoint(this, VSCAN);
		//cerr << "removing predicted VSCAN sync point\n";
	}

	// Calculate moment in time display end occurs.
	vScanSyncTime = frameStartTime + (displayStart +
			( controlRegs[9] & 0x80
			? 212 * TICKS_PER_LINE
			: 192 * TICKS_PER_LINE
			)
		);
	//cerr << "new VSCAN is " << (frameStartTime.getTicksTill(vScanSyncTime) / TICKS_PER_LINE) << "\n";

	// Register new VSCAN sync point.
	if (vScanSyncTime > time) {
		Scheduler::instance()->setSyncPoint(vScanSyncTime, this, VSCAN);
		//cerr << "inserting new VSCAN sync point\n";
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
	// Display line counter continues into the next frame.
	// Note that this implementation is not 100% accurate, since the
	// number of ticks of the *previous* frame should be subtracted.
	// By switching from NTSC to PAL it may even be possible to get two
	// HSCANs in a single frame without modifying any other setting.
	// Fortunately, no known program relies on this.
	int ticksPerFrame = getTicksPerFrame();
	if (horizontalScanOffset >= ticksPerFrame) {
		horizontalScanOffset -= ticksPerFrame;
		// Display line counter is reset at the start of the top border.
		// Any HSCAN that has a higher line number never occurs.
		if (horizontalScanOffset >= LINE_COUNT_RESET_TICKS) {
			// This is one way to say "never".
			horizontalScanOffset = -1000 * TICKS_PER_LINE;
		}
	}

	// Register new HSCAN sync point if interrupt is enabled.
	if ((controlRegs[0] & 0x10) && horizontalScanOffset >= 0) {
		// No line interrupt will occur after bottom erase.
		// NOT TRUE: "after next top border start" is correct.
		// Note that line interrupt can occur in the next frame.
		/*
		EmuTime bottomEraseTime =
			frameStartTime + getTicksPerFrame() - 3 * TICKS_PER_LINE;
		*/
		hScanSyncTime = frameStartTime + horizontalScanOffset;
		if (hScanSyncTime > time) {
			Scheduler::instance()->setSyncPoint(hScanSyncTime, this, HSCAN);
		}
	}
}

// TODO: inline?
// TODO: Is it possible to get rid of this routine and its sync point?
//       VSYNC, HSYNC and DISPLAY_START could be scheduled for the next
//       frame when their callback occurs.
//       But I'm not sure how to handle the PAL/NTSC setting (which also
//       influences the frequency at which E/O toggles).
void VDP::frameStart(const EmuTime &time)
{
	//cerr << "VDP::frameStart @ " << time << "\n";

	// Toggle E/O.
	// Actually this should occur half a line earlier,
	// but for now this is accurate enough.
	statusReg2 ^= 0x02;

	// Settings which are fixed at start of frame.
	// Not sure this is how real MSX does it, but close enough for now.
	// TODO: verticalAdjust probably influences display start, which is
	//       "fixed" once it occured, no need to fix verticalAdjust,
	//       maybe even having this variable is not necessary.
	// TODO: Interlace is effectuated in border height, according to
	//       the data book. Exactly when is the fixation point?
	palTiming = controlRegs[9] & 0x02;
	interlaced = controlRegs[9] & 0x04;
	verticalAdjust = (controlRegs[18] >> 4) ^ 0x07;
	spriteLine = 0;

	// Blinking.
	if (blinkCount != 0) { // counter active?
		blinkCount--;
		if (blinkCount == 0) {
			renderer->updateBlinkState(!blinkState, time);
			blinkState = !blinkState;
			blinkCount = ( blinkState
				? controlRegs[13] >> 4 : controlRegs[13] & 0x0F ) * 10;
		}
	}

	// Schedule next VSYNC.
	frameStartTime = time;
	Scheduler::instance()->setSyncPoint(
		frameStartTime + getTicksPerFrame(), this, VSYNC);
	// Schedule DISPLAY_START, VSCAN and HSCAN.
	scheduleDisplayStart(time);

	// Inform Renderer.
	renderer->frameStart(time);

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
	if (limit > 256) limit = 256;
	for ( ; spriteLine < limit; spriteLine++) {
		spriteCount[spriteLine] =
			checkSprites1(spriteLine, spriteBuffer[spriteLine]);
	}
}

void VDP::updateSprites2(int limit)
{
	// TODO: Replace by display / blank check.
	if (limit > 256) limit = 256;
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
		// Then sync with the Renderer and commit the change.
		if (isPlanar()) {
			setVRAM(((addr << 16) | (addr >> 1)) & vramMask, value, time);
		} else {
			setVRAM(addr, value, time);
		}

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
			int grb = ((value << 8) | paletteLatch) & 0x777;
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
	int addr = (controlRegs[14] << 14) | vramPointer;
	readAhead = getVRAMReordered(addr);
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
			irqVertical.reset();
			return ret;
		}
		case 1: {
			/*
			int ticksThisFrame = getTicksThisFrame(time);
			cout << "S#1 read at (" << (ticksThisFrame % TICKS_PER_LINE)
				<< "," << ((ticksThisFrame - displayStart) / TICKS_PER_LINE)
				<< "), IRQ_H = " << (int)irqHorizontal.getState()
				<< " IRQ_V = " << (int)irqVertical.getState()
				<< ", frame = " << frameStartTime << "\n";
			*/

			if (controlRegs[0] & 0x10) { // line int enabled
				byte ret = statusReg1 | irqHorizontal.getState();
				irqHorizontal.reset();
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
			//       --> Is isDisplayArea actually !VR?
			int displayEnd =
				displayStart + getNumberOfLines() * TICKS_PER_LINE;
			bool vr = ticksThisFrame < displayStart - TICKS_PER_LINE
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
	//PRT_DEBUG("VDP[" << (int)reg << "] = " << std::hex << (int)val << std::dec);

	if (reg>=47) {
		// ignore non-existing registers
		return;
	}
	if (reg>=32) {
		// Pass command register writes to command engine.
		cmdEngine->setCmdReg(reg - 32, val, time);
		return;
	}

	val &= controlValueMasks[reg];
	byte oldval = controlRegs[reg];
	byte change = val ^ oldval;

	// Register 13 is special because writing it resets blinking state,
	// even if the value in the register doesn't change.
	if (reg == 13) {
		// Switch to ON state unless ON period is zero.
		if (blinkState == ((val & 0xF0) == 0)) {
			renderer->updateBlinkState(!blinkState, time);
			blinkState = !blinkState;
		}

		if ((val & 0xF0) && (val & 0x0F)) {
			// Alternating colours, start with ON.
			blinkCount = (val >> 4) * 10;
		} else {
			// Stable colour.
			blinkCount = 0;
		}
	}

	if (!change) return;

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
		// TODO: Reset vertical IRQ if IE0 is reset?
		if (change & 0x18) {
			updateDisplayMode(controlRegs[0], val, time);
		}
		if (change & 0x40) {
			bool newDisplayEnabled = isDisplayArea && (val & 0x40);
			if (newDisplayEnabled != isDisplayEnabled()) {
				renderer->updateDisplayEnabled(newDisplayEnabled, time);
			}
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
		break;
	}
	case 11: {
		updateSprites(time);
		int addr = ((val << 15) | (controlRegs[5] << 7)) & vramMask;
		renderer->updateSpriteAttributeBase(addr, time);
		spriteAttributeBase = addr;
		break;
	}
	case 6: {
		updateSprites(time);
		int addr = (val << 11) & vramMask;
		renderer->updateSpritePatternBase(addr, time);
		spritePatternBase = addr;
		break;
	}
	case 7:
		if (change & 0xF0) {
			renderer->updateForegroundColour(val >> 4, time);
		}
		if (change & 0x0F) {
			renderer->updateBackgroundColour(val & 0x0F, time);
		}
		break;
	case 8:
		if (change & 0x20) {
			renderer->updateTransparency((val & 0x20) == 0, time);
		}
		break;
	case 12:
		if (change & 0xF0) {
			renderer->updateBlinkForegroundColour(val >> 4, time);
		}
		if (change & 0x0F) {
			renderer->updateBlinkBackgroundColour(val & 0x0F, time);
		}
		break;
	case 16:
		// Any half-finished palette loads are aborted.
		paletteLatch = -1;
		break;
	case 18:
		if (change & 0x0F) {
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
				irqHorizontal.reset();
			}
		}
		break;
	case 1:
		if (change & 0x20) { // IE0
			if (!(val & 0x20)) irqVertical.reset();
		}
		break;
	case 9:
		if (change & 0x80) {
			/*
			cerr << "changed to " << (val & 0x80 ? 212 : 192) << " lines"
				<< " at line " << (getTicksThisFrame(time) / TICKS_PER_LINE) << "\n";
			*/
			// Display lines (192/212) determines display start and end.
			// TODO: Find out exactly when display start is fixed.
			//       If it is fixed at VSYNC that would simplify things,
			//       but I think it's more likely the current
			//       implementation is accurate.
			if (time < displayStartSyncTime) {
				// Display start is not fixed yet.
				scheduleDisplayStart(time);
			} else {
				// Display start is fixed, but display end is not.
				scheduleVScan(time);
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
		// TODO: Why didn't I implement this yet?
		//       It's one line of code and overhead is not huge either.
	}
}

VDP::SpritePattern VDP::doublePattern(VDP::SpritePattern a)
{
	// bit-pattern "abcd" gets expanded to "aabbccdd"
	a =   a                  | (a>>16);
	a = ((a<< 8)&0x00ffff00) | (a&0xff0000ff);
	a = ((a<< 4)&0x0ff00ff0) | (a&0xf00ff00f);
	a = ((a<< 2)&0x3c3c3c3c) | (a&0xc3c3c3c3);
	a = ((a<< 1)&0x66666666) | (a&0x99999999);
	return a;
}

VDP::PaletteCmd::PaletteCmd(VDP *vdp)
{
	this->vdp = vdp;
}

void VDP::PaletteCmd::execute(const std::vector<std::string> &tokens)
{
	// Print palette in 4x4 table.
	std::ostringstream out;
	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			int i = col * 4 + row;
			int grb = vdp->getPalette(i);
			out << std::hex << i << std::dec << ":"
				<< ((grb >> 4) & 7) << ((grb >> 8) & 7) << (grb & 7)
				<< "  ";
		}
		out << "\n";
	}
	Console::instance()->print(out.str());
}

void VDP::PaletteCmd::help   (const std::vector<std::string> &tokens)
{
	Console::instance()->print("Prints the current VDP palette (i:rgb).");
}
