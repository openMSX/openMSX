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

#include "VDP.hh"
#include "VDPVRAM.hh"
#include "VDPCmdEngine.hh"
#include "SpriteChecker.hh"
#include "PlatformFactory.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include <string>
#include <iomanip>
#include <cassert>


// Inlined methods first, to make sure they are actually inlined:
// TODO: None left?

// Init and cleanup:

VDP::VDP(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
	, vdpRegsCmd(this)
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
	vram = new VDPVRAM(this, vramSize);

	// Put VDP into reset state, but do not call Renderer methods.
	resetInit(time);

	// Create sprite checker.
	spriteChecker = new SpriteChecker(this, time);
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

	// Tell the other subsystems of the new mask values.
	resetMasks(time);

	// Init scheduling.
	displayStartSyncTime = time;
	vScanSyncTime = time;
	hScanSyncTime = time;
	frameStart(time);

	// Register console commands.
	CommandController::instance()->registerCommand(vdpRegsCmd, "vdpregs");
	CommandController::instance()->registerCommand(paletteCmd, "palette");
	CommandController::instance()->registerCommand(rendererCmd, "renderer");
}

VDP::~VDP()
{
	PRT_DEBUG("Destroying a VDP object");
	CommandController::instance()->unregisterCommand("vdpregs");
	CommandController::instance()->unregisterCommand("palette");
	CommandController::instance()->unregisterCommand("renderer");
	delete cmdEngine;
	delete renderer;
	delete spriteChecker;
	delete vram;
}

void VDP::resetInit(const EmuTime &time)
{
	for (int i = 0; i < 32; i++) controlRegs[i] = 0;
	if (version == TMS9929A) {
		// Boots (and remains) in PAL mode, all other VDPs boot in NTSC.
		controlRegs[9] |= 0x02;
	}
	displayMode = 0;
	vramPointer = 0;
	readAhead = 0;
	dataLatch = 0;
	registerDataStored = false;
	paletteDataStored = false;
	blinkState = false;
	blinkCount = 0;
	horizontalAdjust = 7;
	verticalAdjust = 0;

	// TODO: Real VDP probably resets timing as well.
	isDisplayArea = false;

	// Init status registers.
	statusReg0 = 0x00;
	statusReg1 = (version == V9958 ? 0x04 : 0x00);
	statusReg2 = 0x0C;

	// Update IRQ to reflect new register values.
	irqVertical.reset();
	irqHorizontal.reset();

	// From appendix 8 of the V9938 data book (page 148).
	const word V9938_PALETTE[16] = {
		0x000, 0x000, 0x611, 0x733, 0x117, 0x327, 0x151, 0x627,
		0x171, 0x373, 0x661, 0x664, 0x411, 0x265, 0x555, 0x777
	};
	// Init the palette.
	memcpy(palette, V9938_PALETTE, 16 * sizeof(word));

}

void VDP::resetMasks(const EmuTime &time)
{
	// TODO: Use the updateNameBase method instead of duplicating the effort
	//       here for the initial state.
	vram->nameTable.setMask(~(-1 << 10), -1 << 17, time);
	updateColourBase(time);
	updatePatternBase(time);
	updateSpriteAttributeBase(time);
	updateSpritePatternBase(time);
	// TODO: It is not clear to me yet how bitmapWindow should be used.
	//       Currently it always spans 128K of VRAM.
	vram->bitmapWindow.setMask(~(-1 << 17), -1 << 17, time);
}

void VDP::reset(const EmuTime &time)
{
	resetInit(time);
	renderer->reset(time);
	cmdEngine->reset(time);
	spriteChecker->reset(time);
	// vram->reset(time);	// not necessary
	resetMasks(time);
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
		     (userData == VSCAN ? "VSCAN" :
		     (userData == HSCAN ? "HSCAN" : "DISPLAY_START")))
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
			isDisplayArea = true;
			if (isDisplayEnabled()) {
				vram->updateDisplayEnabled(true, time);
			}
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
	case HOR_ADJUST: {
		int newHorAdjust = (controlRegs[18] & 0x0F) ^ 0x07;
		if (controlRegs[25] & 0x08) {
			newHorAdjust += 4;
		}
		renderer->updateHorizontalAdjust(newHorAdjust, time);
		horizontalAdjust = newHorAdjust;
		break;
	}
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
		: lineZero
		) * TICKS_PER_LINE;
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
	interlaced = controlRegs[9] & 0x08;
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
	assert(isInsideFrame(time));
	switch (port & 0x03) {
	case 0: { // VRAM data write
		int addr = ((controlRegs[14] << 14) | vramPointer) & vramMask;
		//fprintf(stderr, "VRAM[%05X]=%02X\n", addr, value);
		// TODO: Check MXC bit (R#45, bit 6) for extension RAM access.
		//       This bit is kept by the command engine.
		if (isPlanar()) addr = ((addr << 16) | (addr >> 1)) & 0x1FFFF;
		vram->cpuWrite(addr, value, time);
		vramPointer = (vramPointer + 1) & 0x3FFF;
		if (vramPointer == 0 && (displayMode & 0x18)) {
			// In MSX2 video modes, pointer range is 128K.
			controlRegs[14] = (controlRegs[14] + 1) & 0x07;
		}
		readAhead = value;
		registerDataStored = false;
		break;
	}
	case 1: // Register or address write
		if (registerDataStored) {
			if (value & 0x80) {
				// Register write.
				changeRegister(
					value & controlRegMask,
					dataLatch,
					time
					);
			} else {
				// Set read/write address.
				vramPointer = ((word)value << 8 | dataLatch) & 0x3FFF;
				if (!(value & 0x40)) {
					// Read ahead.
					vramRead(time);
				}
			}
			registerDataStored = false;
		} else {
			dataLatch = value;
			registerDataStored = true;
		}
		break;
	case 2: // Palette data write
		if (paletteDataStored) {
			int index = controlRegs[16];
			int grb = ((value << 8) | dataLatch) & 0x777;
			renderer->updatePalette(index, grb, time);
			palette[index] = grb;
			controlRegs[16] = (index + 1) & 0x0F;
			paletteDataStored = false;
		} else {
			dataLatch = value;
			paletteDataStored = true;
		}
		break;
	case 3: { // Indirect register write
		dataLatch = value;
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
	}
}

byte VDP::vramRead(const EmuTime &time)
{
	byte ret = readAhead;
	int addr = (controlRegs[14] << 14) | vramPointer;
	if (isPlanar()) addr = ((addr << 16) | (addr >> 1)) & 0x1FFFF;
	readAhead = vram->cpuRead(addr, time);
	vramPointer = (vramPointer + 1) & 0x3FFF;
	if (vramPointer == 0 && (displayMode & 0x18)) {
		// In MSX2 video mode, pointer range is 128K.
		controlRegs[14] = (controlRegs[14] + 1) & 0x07;
	}
	registerDataStored = false;
	return ret;
}

byte VDP::readIO(byte port, const EmuTime &time)
{
	assert(isInsideFrame(time));
	switch (port & 0x03) {
	case 0: // VRAM data read
		return vramRead(time);
	case 1: // Status register read

		// Abort any port #1 writes in progress.
		registerDataStored = false;

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
			return (byte)spriteChecker->getCollisionX(time);
		case 4:
			return (byte)(spriteChecker->getCollisionX(time) >> 8) | 0xFE;
		case 5: {
			byte ret = (byte)spriteChecker->getCollisionY(time);
			spriteChecker->resetCollision();
			return ret;
		}
		case 6:
			return (byte)(spriteChecker->getCollisionY(time) >> 8) | 0xFC;
		case 7:
			return cmdEngine->readColour(time);
		case 8:
			return (byte)cmdEngine->getBorderX(time);
		case 9:
			return (byte)(cmdEngine->getBorderX(time) >> 8) | 0xFE;
		default: // non-existent status register
			return 0xFF;
		}
	default:
		// These ports should not be registered for reading.
		assert(false);
		return 0xFF;
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
		if (change & 0x0E) {
			updateDisplayMode(val, controlRegs[1], controlRegs[25],
			                  time);
		}
		break;
	case 1:
		if (change & 0x03) {
			// Update sprites on size and mag changes.
			spriteChecker->updateSpriteSizeMag(val, time);
		}
		// TODO: Reset vertical IRQ if IE0 is reset?
		if (change & 0x18) {
			updateDisplayMode(controlRegs[0], val, controlRegs[25],
			                  time);
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
		// TODO:
		// I reverted this fix.
		// Although the code is correct, there is also a counterpart in the
		// renderer that must be updated. I'm too tired now to find it.
		// Since name table checking is currently disabled anyway, keeping the
		// old code does not hurt.
		// Eventually this line should be re-enabled.
		//if (isPlanar()) base = ((base << 16) | (base >> 1)) & 0x1FFFF;
		renderer->updateNameBase(base, time);
		// TODO: Actual number of index bits is lower than 17.
		vram->nameTable.setMask(base, -1 << 17, time);
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
		paletteDataStored = false;
		break;
	case 18:
		if (change & 0x0F) {
			setHorAdjust(time);
		}
		break;
	case 23:
		spriteChecker->updateVerticalScroll(val, time);
		renderer->updateVerticalScroll(val, time);
		break;
	case 25:
		if (change & 0x18) {
			updateDisplayMode(controlRegs[0], controlRegs[1], val,
			                  time);
		}
		if (change & 0x08) {
			setHorAdjust(time);
		}
		break;
	}

	// Commit the change.
	controlRegs[reg] = val;

	// Perform additional tasks after new value became active.
	// Because base masks cannot be read from the VDP, updating them after
	// the commit is equivalent to updating before.
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
	case 3:
	case 10:
		updateColourBase(time);
		break;
	case 4:
		updatePatternBase(time);
		break;
	case 5:
	case 11:
		updateSpriteAttributeBase(time);
		break;
	case 6:
		updateSpritePatternBase(time);
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

void VDP::setHorAdjust(const EmuTime &time)
{
	int line = getTicksThisFrame(time) / TICKS_PER_LINE;
	int ticks = (line + 1) * TICKS_PER_LINE;
	EmuTime nextTime = frameStartTime + ticks;
	Scheduler::instance()->setSyncPoint(nextTime, this, HOR_ADJUST);
}

void VDP::updateColourBase(const EmuTime &time)
{
	int base = vramMask &
		((controlRegs[10] << 14) | (controlRegs[3] << 6) | ~(-1 << 6));
	renderer->updateColourBase(base, time);
	switch (displayMode & 0x1F) {
	case 0x09: // Text 2.
		// TODO: Enable this only if dual color is actually active.
		vram->colourTable.setMask(base, -1 << 9, time);
		break;
	case 0x00: // Graphic 1.
		vram->colourTable.setMask(base, -1 << 6, time);
		break;
	case 0x04: // Graphic 2.
	case 0x08: // Graphic 3.
		vram->colourTable.setMask(base, -1 << 13, time);
		break;
	default:
		// Other display modes do not use a colour table.
		vram->colourTable.disable(time);
	}
}

void VDP::updatePatternBase(const EmuTime &time)
{
	int base = vramMask & ((controlRegs[4] << 11) | ~(-1 << 11));
	renderer->updatePatternBase(base, time);
	switch (displayMode & 0x1F) {
	case 0x01: // Text 1.
	case 0x05: // Text 1 Q.
	case 0x09: // Text 2.
	case 0x00: // Graphic 1.
	case 0x02: // Multicolour.
	case 0x06: // Multicolour Q.
		vram->patternTable.setMask(base, -1 << 11, time);
		break;
	case 0x04: // Graphic 2.
	case 0x08: // Graphic 3.
		vram->patternTable.setMask(base, -1 << 13, time);
		break;
	default:
		// Other display modes do not use a pattern table.
		vram->patternTable.disable(time);
	}
}

void VDP::updateSpriteAttributeBase(const EmuTime &time)
{
	int mode = getSpriteMode();
	if (mode == 0) {
		vram->spriteAttribTable.disable(time);
		return;
	}
	int base = vramMask &
		((controlRegs[11] << 15) | (controlRegs[5] << 7) | ~(-1 << 7));
	if (mode == 1) {
		vram->spriteAttribTable.setMask(base, -1 << 7, time);
	} else { // mode == 2
		if (isPlanar()) {
			vram->spriteAttribTable.setMask(
				((base << 16) | (base >> 1)) & 0x1FFFF, 0x0FE00, time);
		} else {
			vram->spriteAttribTable.setMask(base, 0x1FC00, time);
		}
	}
}

void VDP::updateSpritePatternBase(const EmuTime &time)
{
	int base = ((controlRegs[6] << 11) | ~(-1 << 11)) & vramMask;
	if (isPlanar()) base = ((base << 16) | (base >> 1)) & 0x1FFFF;
	vram->spritePatternTable.setMask(base, -1 << 11, time);
}

void VDP::updateDisplayMode(
	byte reg0, byte reg1, byte reg25, const EmuTime &time )
{
	int newMode =
		  ((reg25 & 0x18) << 2)  // YAE YJK
		| ((reg0  & 0x0E) << 1)  // M5..M3
		| ((reg1  & 0x08) >> 2)  // M2
		| ((reg1  & 0x10) >> 4); // M1
	if (newMode != displayMode) {
		//PRT_DEBUG("VDP: mode " << newMode);

		// Synchronise subsystems.
		vram->updateDisplayMode(newMode, time);

		// TODO: Is this a useful optimisation, or doesn't it help
		//       in practice?
		// What aspects have changed:
		// Switched from planar to nonplanar or vice versa.
		bool planarChange = isPlanar(newMode) != isPlanar();
		// Sprite mode changed.
		bool spriteModeChange = getSpriteMode(newMode) != getSpriteMode();

		// Commit the new display mode.
		displayMode = newMode;

		updateColourBase(time);
		updatePatternBase(time);
		if (planarChange) {
			updateSpritePatternBase(time);
		}
		if (planarChange || spriteModeChange) {
			updateSpriteAttributeBase(time);
		}

		// To be extremely accurate, reschedule hscan when changing
		// from/to text mode. Text mode has different border width,
		// which affects the moment hscan occurs.
		// TODO: Why didn't I implement this yet?
		//       It's one line of code and overhead is not huge either.
	}
}

// VDPRegsCmd inner class:

VDP::VDPRegsCmd::VDPRegsCmd(VDP *vdp)
{
	this->vdp = vdp;
}

void VDP::VDPRegsCmd::execute(const std::vector<std::string> &tokens,
                              const EmuTime &time)
{
	// Print palette in 4x4 table.
	std::ostringstream out;
	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 4; col++) {
			int reg = col * 8 + row;
			int value = vdp->controlRegs[reg];
			out << std::dec << std::setw(2) << reg;
			out << " : ";
			out << std::hex << std::setw(2) << value;
			out << "   ";
		}
		out << "\n";
	}
	print(out.str());
}

void VDP::VDPRegsCmd::help(const std::vector<std::string> &tokens) const
{
	print("Prints the current state of the VDP registers.");
}

// PaletteCmd inner class:

VDP::PaletteCmd::PaletteCmd(VDP *vdp)
{
	this->vdp = vdp;
}

void VDP::PaletteCmd::execute(const std::vector<std::string> &tokens,
                              const EmuTime &time)
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

void VDP::PaletteCmd::help(const std::vector<std::string> &tokens) const
{
	print("Prints the current VDP palette (i:rgb).");
}

// RendererCmd inner class:

VDP::RendererCmd::RendererCmd(VDP *vdp)
{
	this->vdp = vdp;
}

void VDP::RendererCmd::execute(const std::vector<std::string> &tokens,
                               const EmuTime &time)
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

void VDP::RendererCmd::help(const std::vector<std::string> &tokens) const
{
	print("Select a new renderer or print the current renderer.");
}
