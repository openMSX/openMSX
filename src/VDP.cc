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
- Check how Z80 should treat interrupts occurring during DI.
- Get rid of hardcoded port 0x98..0x9B.
- Bottom erase suspends display even on overscan.
  However, it t shows black, not border colour.
  How to handle this? Currently it is treated as "overscan" which
  falls outside of the rendered screen area.
*/

#include "MSXMotherBoard.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "VDPCmdEngine.hh"
#include "SpriteChecker.hh"
#include "PlatformFactory.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include <string>
#include <cassert>


// Inlined methods first, to make sure they are actually inlined:
// TODO: None left?

// Init and cleanup:

VDP::VDP(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
	, paletteCmd(this)
	, rendererCmd(this)
{
	PRT_DEBUG("Creating a VDP object");

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
	vram = new VDPVRAM(vramSize);

	// Put VDP into reset state, but do not call Renderer methods.
	resetInit(time);

	// Create sprite checker.
	bool limitSprites = deviceConfig->getParameterAsBool("limit_sprites");
	spriteChecker = new SpriteChecker(this, limitSprites, time);
	vram->setSpriteChecker(spriteChecker);

	// Create command engine.
	cmdEngine = new VDPCmdEngine(this, time);
	vram->setCmdEngine(cmdEngine);

	// Get renderer type and parameters from config.
	MSXConfig::Config *renderConfig =
		MSXConfig::Backend::instance()->getConfigById("renderer");
	bool fullScreen = renderConfig->getParameterAsBool("full_screen");
	rendererName = renderConfig->getType();
	// Create renderer.
	renderer = PlatformFactory::createRenderer(
		rendererName, this, fullScreen, time);
	vram->setRenderer(renderer);
	switchRenderer = false;

	MSXMotherBoard::instance()->register_IO_In((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x98, this);
	MSXMotherBoard::instance()->register_IO_In((byte)0x99, this);
	MSXMotherBoard::instance()->register_IO_Out((byte)0x99, this);
	if (!isMSX1VDP()) {
		MSXMotherBoard::instance()->register_IO_Out((byte)0x9A, this);
		MSXMotherBoard::instance()->register_IO_Out((byte)0x9B, this);
	}

	// Init scheduling.
	displayStartSyncTime = time;
	vScanSyncTime = time;
	hScanSyncTime = time;
	frameStart(time);

	// Register console commands.
	CommandController::instance()->registerCommand(paletteCmd, "palette");
	CommandController::instance()->registerCommand(rendererCmd, "renderer");
}

VDP::~VDP()
{
	PRT_DEBUG("Destroying a VDP object");
	CommandController::instance()->unregisterCommand("palette");
	CommandController::instance()->unregisterCommand("renderer");
	delete(cmdEngine);
	delete(renderer);
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

	// Update IRQ to reflect new register values.
	irqVertical.reset();
	irqHorizontal.reset();

	// Initially the table regs are zero, so every mask bit that is
	// controlled by a the table reg should be zero.
	// See Register Functions 1.2 in the V9938 data book (page 5).
	vram->nameTable.setMask(~(-1 << 10), 17);
	vram->colourTable.setMask(~(-1 << 6), 17);
	vram->patternTable.setMask(~(-1 << 11), 17);
	vram->spriteAttribTable.setMask(~(-1 << 7), 17);
	vram->spritePatternTable.setMask(~(-1 << 11), 17);
	// TODO: It is not clear to me yet how bitmapWindow should be used.
	//       Currently it always spans 128K of VRAM.
	vram->bitmapWindow.setMask(~(-1 << 17), 17);

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
	std::cout << (userData == VSYNC ? "VSYNC" :
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
				vram->updateDisplayEnabled(true, time);
			}
			isDisplayArea = true;
		}
		break;
	case VSCAN:
		// VSCAN is the end of display.
		if (isDisplayEnabled()) {
			vram->updateDisplayEnabled(false, time);
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
	vScanSyncTime = frameStartTime +
	                (displayStart + getNumberOfLines() * TICKS_PER_LINE);
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

	// Inform VDP subcomponents.
	// TODO: Do this via VDPVRAM?
	renderer->frameStart(time);
	spriteChecker->frameStart(time);

	if (switchRenderer) {
		switchRenderer = false;
		PRT_DEBUG("VDP: switching renderer to " << rendererName);
		bool fullScreen = renderer->isFullScreen();
		delete renderer;
		// TODO: Handle invalid names more gracefully.
		renderer = PlatformFactory::createRenderer(
			rendererName, this, fullScreen, time);
		vram->setRenderer(renderer);
	}

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

	/*
	   std::cout << "--> frameStart = " << frameStartTime
		<< ", frameEnd = " << (frameStartTime + getTicksPerFrame())
		<< ", hscan = " << hScanSyncTime
		<< ", displayStart = " << displayStart
		<< ", timing: " << (palTiming ? "PAL" : "NTSC")
		<< "\n";
	*/
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
		vram->cpuWrite(addr, value, time);
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
					vramRead(time);
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

byte VDP::vramRead(const EmuTime &time)
{
	byte ret = readAhead;
	int addr = (controlRegs[14] << 14) | vramPointer;
	readAhead = vram->cpuRead(addr, time);
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
		return vramRead(time);
	}
	else { // port == 0x99

		// Abort any port 0x99 writes in progress.
		firstByte = -1;

		//std::cout << "read S#" << (int)controlRegs[15] << "\n";

		// Calculate status register contents.
		switch (controlRegs[15]) {
		case 0: {
			byte ret = statusReg0 | spriteChecker->readStatus(time);
			statusReg0 = 0;
			irqVertical.reset();
			return ret;
		}
		case 1: {
			/*
			int ticksThisFrame = getTicksThisFrame(time);
			std::cout << "S#1 read at (" << (ticksThisFrame % TICKS_PER_LINE)
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
			return (byte)spriteChecker->getCollisionX();
		case 4:
			return (byte)(spriteChecker->getCollisionX() >> 8) | 0xFE;
		case 5: {
			byte ret = (byte)spriteChecker->getCollisionY();
			spriteChecker->resetCollision();
			return ret;
		}
		case 6:
			return (byte)(spriteChecker->getCollisionY() >> 8) | 0xFC;
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
			updateDisplayMode(val, controlRegs[1], time);
		}
		break;
	case 1:
		if (change & 0x03) {
			// Update sprites on size and mag changes.
			spriteChecker->updateSpriteSizeMag(val, time);
		}
		// TODO: Reset vertical IRQ if IE0 is reset?
		if (change & 0x18) {
			updateDisplayMode(controlRegs[0], val, time);
		}
		if (change & 0x40) {
			bool newDisplayEnabled = isDisplayArea && (val & 0x40);
			if (newDisplayEnabled != isDisplayEnabled()) {
				vram->updateDisplayEnabled(newDisplayEnabled, time);
			}
		}
		break;
	case 2: {
		int base = ((val << 10) | ~(-1 << 10)) & vramMask;
		renderer->updateNameBase(base, time);
		// TODO: Actual number of index bits is lower than 17.
		vram->nameTable.setMask(base, 17);
		break;
	}
	case 3: {
		int base = ((controlRegs[10] << 14) | (val << 6)
			| ~(-1 << 6)) & vramMask;
		renderer->updateColourBase(base, time);
		// TODO: Actual number of index bits is lower than 17.
		vram->colourTable.setMask(base, 17);
		break;
	}
	case 10: {
		int base = ((val << 14) | (controlRegs[3] << 6)
			| ~(-1 << 6)) & vramMask;
		renderer->updateColourBase(base, time);
		// TODO: Actual number of index bits is lower than 17.
		vram->colourTable.setMask(base, 17);
		break;
	}
	case 4: {
		int base = ((val << 11) | ~(-1 << 11)) & vramMask;
		renderer->updatePatternBase(base, time);
		// TODO: Actual number of index bits is lower than 17.
		vram->patternTable.setMask(base, 17);
		break;
	}
	case 5: {
		int base = ((controlRegs[11] << 15) | (val << 7)
			| ~(-1 << 7)) & vramMask;
		spriteChecker->updateSpriteAttributeBase(base, time);
		// TODO: Actual number of index bits is lower than 17.
		vram->spriteAttribTable.setMask(base, 17);
		break;
	}
	case 11: {
		int base = ((val << 15) | (controlRegs[5] << 7)
			| ~(-1 << 7)) & vramMask;
		spriteChecker->updateSpriteAttributeBase(base, time);
		// TODO: Actual number of index bits is lower than 17.
		vram->spriteAttribTable.setMask(base, 17);
		break;
	}
	case 6: {
		int base = ((val << 11) | ~(-1 << 11)) & vramMask;
		spriteChecker->updateSpritePatternBase(base, time);
		vram->spritePatternTable.setMask(base, 11);
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
		if (change & 0x02) {
			vram->updateSpritesEnabled((val & 0x02) == 0, time);
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
		spriteChecker->updateVerticalScroll(val, time);
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
		vram->updateDisplayMode(newMode, time);
		displayMode = newMode;
		// To be extremely accurate, reschedule hscan when changing
		// from/to text mode. Text mode has different border width,
		// which affects the moment hscan occurs.
		// TODO: Why didn't I implement this yet?
		//       It's one line of code and overhead is not huge either.
	}
}

// PaletteCmd inner class:

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
	print(out.str());
}

void VDP::PaletteCmd::help(const std::vector<std::string> &tokens)
{
	print("Prints the current VDP palette (i:rgb).");
}

// RendererCmd inner class:

VDP::RendererCmd::RendererCmd(VDP *vdp)
{
	this->vdp = vdp;
}

void VDP::RendererCmd::execute(const std::vector<std::string> &tokens)
{
	switch (tokens.size()) {
		case 1:
			// Print name of current renderer.
			print("Current renderer: " + vdp->rendererName);
			break;
		case 2:
			// Switch renderer.
			vdp->rendererName = tokens[1];
			vdp->switchRenderer = true;
			break;
		default:
			throw CommandException("Too many parameters.");
	}
}

void VDP::RendererCmd::help(const std::vector<std::string> &tokens)
{
	print("Select a new renderer or print the current renderer.");
}
