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
- Bottom erase suspends display even on overscan.
  However, it shows black, not border color.
  How to handle this? Currently it is treated as "overscan" which
  falls outside of the rendered screen area.
*/

#include "VDP.hh"
#include "VDPVRAM.hh"
#include "VDPCmdEngine.hh"
#include "SpriteChecker.hh"
#include "Display.hh"
#include "RendererFactory.hh"
#include "Renderer.hh"
#include "RenderSettings.hh"
#include "EnumSetting.hh"
#include "TclObject.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "MSXException.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include "memory.hh"
#include <cstring>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

VDP::VDP(const DeviceConfig& config)
	: MSXDevice(config)
	, syncVSync(*this)
	, syncDisplayStart(*this)
	, syncVScan(*this)
	, syncHScan(*this)
	, syncHorAdjust(*this)
	, syncSetMode(*this)
	, syncSetBlank(*this)
	, syncCpuVramAccess(*this)
	, display(getReactor().getDisplay())
	, cmdTiming    (display.getRenderSettings().getCmdTimingSetting())
	, tooFastAccess(display.getRenderSettings().getTooFastAccessSetting())
	, vdpRegDebug      (*this)
	, vdpStatusRegDebug(*this)
	, vdpPaletteDebug  (*this)
	, vramPointerDebug (*this)
	, frameCountInfo   (*this)
	, cycleInFrameInfo (*this)
	, lineInFrameInfo  (*this)
	, cycleInLineInfo  (*this)
	, msxYPosInfo      (*this)
	, msxX256PosInfo   (*this)
	, msxX512PosInfo   (*this)
	, frameStartTime(getCurrentTime())
	, irqVertical  (getMotherBoard(), getName() + ".IRQvertical",   config)
	, irqHorizontal(getMotherBoard(), getName() + ".IRQhorizontal", config)
	, displayStartSyncTime(getCurrentTime())
	, vScanSyncTime(getCurrentTime())
	, hScanSyncTime(getCurrentTime())
	, tooFastCallback(
		getCommandController(),
		getName() + ".too_fast_vram_access_callback",
		"Tcl proc called when the VRAM is read or written too fast")
	, warningPrinted(false)
{
	VDPAccessSlots::initTables();

	interlaced = false;

	std::string versionString = config.getChildData("version");
	if (versionString == "TMS99X8A") version = TMS99X8A;
	else if (versionString == "TMS9918A") version = TMS99X8A;
	else if (versionString == "TMS9928A") version = TMS99X8A;
	else if (versionString == "T6950PAL") version = T6950PAL;
	else if (versionString == "T6950NTSC") version = T6950NTSC;
	else if (versionString == "T7937APAL") version = T7937APAL;
	else if (versionString == "T7937ANTSC") version = T7937ANTSC;
	else if (versionString == "TMS91X8") version = TMS91X8;
	else if (versionString == "TMS9118") version = TMS91X8;
	else if (versionString == "TMS9128") version = TMS91X8;
	else if (versionString == "TMS9929A") version = TMS9929A;
	else if (versionString == "TMS9129") version = TMS9129;
	else if (versionString == "V9938") version = V9938;
	else if (versionString == "V9958") version = V9958;
	else throw MSXException("Unknown VDP version \"" + versionString + "\"");

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
	       isMSX1VDP() ? VALUE_MASKS_MSX1 : VALUE_MASKS_MSX2,
	       sizeof(controlValueMasks));
	if (version == V9958) {
		// Enable V9958-specific control registers.
		controlValueMasks[25] = 0x7F;
		controlValueMasks[26] = 0x3F;
		controlValueMasks[27] = 0x07;
	}

	resetInit(); // must be done early to avoid UMRs

	// Video RAM.
	EmuTime::param time = getCurrentTime();
	unsigned vramSize =
		(isMSX1VDP() ? 16 : config.getChildDataAsInt("vram"));
	if ((vramSize !=  16) && (vramSize !=  64) &&
	    (vramSize != 128) && (vramSize != 192)) {
		throw MSXException(StringOp::Builder() <<
			"VRAM size of " << vramSize << "kB is not supported!");
	}
	vram = make_unique<VDPVRAM>(*this, vramSize * 1024, time);

	// Create sprite checker.
	auto& renderSettings = display.getRenderSettings();
	spriteChecker = make_unique<SpriteChecker>(*this, renderSettings, time);
	vram->setSpriteChecker(spriteChecker.get());

	// Create command engine.
	cmdEngine = make_unique<VDPCmdEngine>(*this, getCommandController());
	vram->setCmdEngine(cmdEngine.get());

	// Initialise renderer.
	createRenderer();

	// Reset state.
	powerUp(time);

	display      .attach(*this);
	cmdTiming    .attach(*this);
	tooFastAccess.attach(*this);
	update(tooFastAccess); // handles both cmdTiming and tooFastAccess
}

VDP::~VDP()
{
	tooFastAccess.detach(*this);
	cmdTiming    .detach(*this);
	display      .detach(*this);
}

void VDP::preVideoSystemChange()
{
	renderer.reset();
}

void VDP::postVideoSystemChange()
{
	createRenderer();
}

void VDP::createRenderer()
{
	renderer = RendererFactory::createRenderer(*this, display);
	// TODO: Is it safe to use frameStartTime,
	//       which is most likely in the past?
	//renderer->reset(frameStartTime.getTime());
	vram->setRenderer(renderer.get(), frameStartTime.getTime());
}

PostProcessor* VDP::getPostProcessor() const
{
	return renderer->getPostProcessor();
}

void VDP::resetInit()
{
	// note: vram, spriteChecker, cmdEngine, renderer may not yet be
	//       created at this point
	for (auto& reg : controlRegs) {
		reg = 0;
	}
	if (isVDPwithPALonly()) {
		// Boots (and remains) in PAL mode, all other VDPs boot in NTSC.
		controlRegs[9] |= 0x02;
	}
	// According to page 6 of the V9938 data book the color burst registers
	// are loaded with these values at power on.
	controlRegs[21] = 0x3B;
	controlRegs[22] = 0x05;
	// Note: frameStart is the actual place palTiming is written, but it
	//       can be read before frameStart is called.
	//       TODO: Clean up initialisation sequence.
	palTiming = true; // controlRegs[9] & 0x02;
	displayMode.reset();
	vramPointer = 0;
	cpuVramData = 0;
	dataLatch = 0;
	cpuExtendedVram = false;
	registerDataStored = false;
	paletteDataStored = false;
	blinkState = false;
	blinkCount = 0;
	horizontalAdjust = 7;

	// TODO: Real VDP probably resets timing as well.
	isDisplayArea = false;
	displayEnabled = false;
	superimposing = nullptr;
	externalVideo = nullptr;

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
	memcpy(palette, V9938_PALETTE, sizeof(V9938_PALETTE));
}

void VDP::resetMasks(EmuTime::param time)
{
	updateNameBase(time);
	updateColorBase(time);
	updatePatternBase(time);
	updateSpriteAttributeBase(time);
	updateSpritePatternBase(time);
	// TODO: It is not clear to me yet how bitmapWindow should be used.
	//       Currently it always spans 128K of VRAM.
	//vram->bitmapWindow.setMask(~(~0u << 17), ~0u << 17, time);
}

void VDP::powerUp(EmuTime::param time)
{
	vram->clear();
	reset(time);
}

void VDP::reset(EmuTime::param time)
{
	syncVSync        .removeSyncPoint();
	syncDisplayStart .removeSyncPoint();
	syncVScan        .removeSyncPoint();
	syncHScan        .removeSyncPoint();
	syncHorAdjust    .removeSyncPoint();
	syncSetMode      .removeSyncPoint();
	syncSetBlank     .removeSyncPoint();
	syncCpuVramAccess.removeSyncPoint();
	pendingCpuAccess = false;

	// Reset subsystems.
	cmdEngine->sync(time);
	resetInit();
	spriteChecker->reset(time);
	cmdEngine->reset(time);
	renderer->reInit();

	// Tell the subsystems of the new mask values.
	resetMasks(time);

	// Init scheduling.
	frameCount = -1;
	frameStart(time);
	assert(frameCount == 0);
}

void VDP::execVSync(EmuTime::param time)
{
	// This frame is finished.
	// Inform VDP subcomponents.
	// TODO: Do this via VDPVRAM?
	renderer->frameEnd(time);
	spriteChecker->frameEnd(time);
	// Start next frame.
	frameStart(time);
}

void VDP::execDisplayStart(EmuTime::param time)
{
	// Display area starts here, unless we're doing overscan and it
	// was already active.
	if (!isDisplayArea) {
		if (displayEnabled) {
			vram->updateDisplayEnabled(true, time);
		}
		isDisplayArea = true;
	}
}

void VDP::execVScan(EmuTime::param time)
{
	// VSCAN is the end of display.
	// This will generate a VBLANK IRQ. Typically MSX software will
	// poll the keyboard/joystick on this IRQ. So now is a good
	// time to also poll for host events.
	getReactor().enterMainLoop();

	if (isDisplayEnabled()) {
		vram->updateDisplayEnabled(false, time);
	}
	isDisplayArea = false;

	// Vertical scanning occurs.
	statusReg0 |= 0x80;
	if (controlRegs[1] & 0x20) {
		irqVertical.set();
	}
}

void VDP::execHScan()
{
	// Horizontal scanning occurs.
	if (controlRegs[0] & 0x10) {
		irqHorizontal.set();
	}
}

void VDP::execHorAdjust(EmuTime::param time)
{
	int newHorAdjust = (controlRegs[18] & 0x0F) ^ 0x07;
	if (controlRegs[25] & 0x08) {
		newHorAdjust += 4;
	}
	renderer->updateHorizontalAdjust(newHorAdjust, time);
	horizontalAdjust = newHorAdjust;
}

void VDP::execSetMode(EmuTime::param time)
{
	updateDisplayMode(
		DisplayMode(controlRegs[0], controlRegs[1], controlRegs[25]),
		time);
}

void VDP::execSetBlank(EmuTime::param time)
{
	bool newDisplayEnabled = (controlRegs[1] & 0x40) != 0;
	if (isDisplayArea) {
		vram->updateDisplayEnabled(newDisplayEnabled, time);
	}
	displayEnabled = newDisplayEnabled;
}

void VDP::execCpuVramAccess(EmuTime::param time)
{
	assert(!allowTooFastAccess);
	pendingCpuAccess = false;
	executeCpuVramAccess(time);
}

// TODO: This approach assumes that an overscan-like approach can be used
//       skip display start, so that the border is rendered instead.
//       This makes sense, but it has not been tested on real MSX yet.
void VDP::scheduleDisplayStart(EmuTime::param time)
{
	// Remove pending DISPLAY_START sync point, if any.
	if (displayStartSyncTime > time) {
		syncDisplayStart.removeSyncPoint();
		//cerr << "removing predicted DISPLAY_START sync point\n";
	}

	// Calculate when (lines and time) display starts.
	int verticalAdjust = (controlRegs[18] >> 4) ^ 0x07;
	int lineZero =
		// sync + top erase:
		3 + 13 +
		// top border:
		(palTiming ? 36 : 9) +
		(controlRegs[9] & 0x80 ? 0 : 10) +
		verticalAdjust;
	displayStart =
		lineZero * TICKS_PER_LINE
		+ 100 + 102; // VR flips at start of left border
	displayStartSyncTime = frameStartTime + displayStart;
	//cerr << "new DISPLAY_START is " << (displayStart / TICKS_PER_LINE) << "\n";

	// Register new DISPLAY_START sync point.
	if (displayStartSyncTime > time) {
		syncDisplayStart.setSyncPoint(displayStartSyncTime);
		//cerr << "inserting new DISPLAY_START sync point\n";
	}

	// HSCAN and VSCAN are relative to display start.
	scheduleHScan(time);
	scheduleVScan(time);
}

void VDP::scheduleVScan(EmuTime::param time)
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
		syncVScan.removeSyncPoint();
		//cerr << "removing predicted VSCAN sync point\n";
	}

	// Calculate moment in time display end occurs.
	vScanSyncTime = frameStartTime +
	                (displayStart + getNumberOfLines() * TICKS_PER_LINE);
	//cerr << "new VSCAN is " << (frameStartTime.getTicksTill(vScanSyncTime) / TICKS_PER_LINE) << "\n";

	// Register new VSCAN sync point.
	if (vScanSyncTime > time) {
		syncVScan.setSyncPoint(vScanSyncTime);
		//cerr << "inserting new VSCAN sync point\n";
	}
}

void VDP::scheduleHScan(EmuTime::param time)
{
	// Remove pending HSCAN sync point, if any.
	if (hScanSyncTime > time) {
		syncHScan.removeSyncPoint();
		hScanSyncTime = time;
	}

	// Calculate moment in time line match occurs.
	horizontalScanOffset = displayStart - (100 + 102)
		+ ((controlRegs[19] - controlRegs[23]) & 0xFF) * TICKS_PER_LINE
		+ getRightBorder();
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
			syncHScan.setSyncPoint(hScanSyncTime);
		}
	}
}

// TODO: inline?
// TODO: Is it possible to get rid of this routine and its sync point?
//       VSYNC, HSYNC and DISPLAY_START could be scheduled for the next
//       frame when their callback occurs.
//       But I'm not sure how to handle the PAL/NTSC setting (which also
//       influences the frequency at which E/O toggles).
void VDP::frameStart(EmuTime::param time)
{
	++frameCount;

	//cerr << "VDP::frameStart @ " << time << "\n";

	// Toggle E/O.
	// Actually this should occur half a line earlier,
	// but for now this is accurate enough.
	statusReg2 ^= 0x02;

	// Settings which are fixed at start of frame.
	// Not sure this is how real MSX does it, but close enough for now.
	// TODO: Interlace is effectuated in border height, according to
	//       the data book. Exactly when is the fixation point?
	palTiming = (controlRegs[9] & 0x02) != 0;
	interlaced = (controlRegs[9] & 0x08) != 0;

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

	// TODO: Presumably this is done here
	// Note that if superimposing is enabled but no external video
	// signal is provided then the VDP stops producing a signal
	// (at least on an MSX1, VDP(0)=1 produces "signal lost" on my
	// monitor)
	const RawFrame* newSuperimposing = (controlRegs[0] & 1) ? externalVideo : nullptr;
	if (superimposing != newSuperimposing) {
		superimposing = newSuperimposing;
		renderer->updateSuperimposing(superimposing, time);
	}

	// Schedule next VSYNC.
	frameStartTime.reset(time);
	syncVSync.setSyncPoint(frameStartTime + getTicksPerFrame());
	// Schedule DISPLAY_START, VSCAN and HSCAN.
	scheduleDisplayStart(time);

	// Inform VDP subcomponents.
	// TODO: Do this via VDPVRAM?
	renderer->frameStart(time);
	spriteChecker->frameStart(time);

	/*
	   cout << "--> frameStart = " << frameStartTime
		<< ", frameEnd = " << (frameStartTime + getTicksPerFrame())
		<< ", hscan = " << hScanSyncTime
		<< ", displayStart = " << displayStart
		<< ", timing: " << (palTiming ? "PAL" : "NTSC")
		<< "\n";
	*/
}

// The I/O functions.

void VDP::writeIO(word port, byte value, EmuTime::param time)
{
	assert(isInsideFrame(time));
	switch (port & (isMSX1VDP() ? 0x01 : 0x03)) {
	case 0: // VRAM data write
		vramWrite(value, time);
		registerDataStored = false;
		break;
	case 1: // Register or address write
		if (registerDataStored) {
			if (value & 0x80) {
				if (!(value & 0x40) || isMSX1VDP()) {
					// Register write.
					changeRegister(
						value & controlRegMask,
						dataLatch,
						time
						);
				} else {
					// TODO what happens in this case?
					// it's not a register write because
					// that breaks "SNOW26" demo
				}
				if (isMSX1VDP()) {
					// For these VDP's the VRAM pointer is modified when
					// writing to VDP registers. Without this some demos won't
					// run as on real MSX1, e.g. Planet of the Epas, Utopia and
					// Waves 1.2. Thanks to dvik for finding this out.
					// See also below about not using the latch on MSX1.
					// Set read/write address.
					vramPointer = (value << 8 | (vramPointer & 0xFF)) & 0x3FFF;
				}
			} else {
				// Set read/write address.
				vramPointer = (value << 8 | dataLatch) & 0x3FFF;
				if (!(value & 0x40)) {
					// Read ahead.
					vramRead(time);
				}
			}
			registerDataStored = false;
		} else {
			// Note: on MSX1 there seems to be no
			// latch used, but VDP address writes
			// are done directly.
			// Thanks to hap for finding this out. :)
			if (isMSX1VDP()) {
				vramPointer = (vramPointer & 0x3F00) | value;
			}
			dataLatch = value;
			registerDataStored = true;
		}
		break;
	case 2: // Palette data write
		if (paletteDataStored) {
			int index = controlRegs[16];
			int grb = ((value << 8) | dataLatch) & 0x777;
			setPalette(index, grb, time);
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

void VDP::setPalette(int index, word grb, EmuTime::param time)
{
	if (palette[index] != grb) {
		renderer->updatePalette(index, grb, time);
		palette[index] = grb;
	}
}

void VDP::vramWrite(byte value, EmuTime::param time)
{
	scheduleCpuVramAccess(false, value, time);
}

byte VDP::vramRead(EmuTime::param time)
{
	// Return the result from a previous read. In case
	// allowTooFastAccess==true, the call to scheduleCpuVramAccess()
	// already overwrites that variable, so make a local copy first.
	byte result = cpuVramData;

	byte dummy = 0;
	scheduleCpuVramAccess(true, dummy, time); // schedule next read
	return result;
}

void VDP::scheduleCpuVramAccess(bool isRead, byte write, EmuTime::param time)
{
	// Tested on real V9938: 'cpuVramData' is shared between read and write.
	// E.g. OUT (#98),A followed by IN A,(#98) returns the just written value.
	if (!isRead) cpuVramData = write;
	cpuVramReqIsRead = isRead;
	if (unlikely(pendingCpuAccess)) {
		// Already scheduled. Do nothing.
		// The old request has been overwritten by the new request!
		assert(!allowTooFastAccess);
		tooFastCallback.execute();
	} else {
		if (unlikely(allowTooFastAccess)) {
			// Immediately execute request.
			// In the past, in allowTooFastAccess-mode, we would
			// still schedule the actual access, but process
			// pending requests early when a new one arrives before
			// the old one was handled. Though this would still go
			// wrong because of the delayed update of
			// 'vramPointer'. We could _only_ _partly_ work around
			// that by calculating the actual vram address early
			// (likely not what the real VDP does). But because
			// allowTooFastAccess is anyway an artifical situation
			// we now solve this in a simpler way: simply not
			// schedule CPU-VRAM accesses.
			assert(!pendingCpuAccess);
			executeCpuVramAccess(time);
		} else {
			// For V99x8 there are 16 extra cycles, for details see:
			//    doc/internal/vdp-vram-timing/vdp-timing.html
			// For TMS99x8 the situation is less clear, see
			//    doc/internal/vdp-vram-timing/vdp-timing-2.html
			// Additional measurements(*) show that picking either 8 or 9
			// TMS cycles (equivalent to 32 or 36 V99x8 cycles) gives the
			// same result as on a real MSX. This corresponds to
			// respectively 1.49us or 1.68us, the TMS documentation
			// specifies 2us for this value.
			//  (*) In this test we did a lot of OUT operations (writes to
			//  VRAM) that are exactly N cycles apart. After the writes we
			//  detect whether all were successful by reading VRAM
			//  (slowly). We vary N and found that you get corruption for
			//  N<=26 cycles, but no corruption occurs for N>=27. This test
			//  was done in screen 2 with 4 sprites visible on one line
			//  (though the sprites did not seem to make a difference).
			// So this test could not decide between 8 or 9 TMS cycles.
			// To be on the safe side we picked 8.
			//
			// Update: 8 cycles (DELTA_32) causes corruption in
			// 'Chase HQ', see
			//    http://www.msx.org/forum/msx-talk/openmsx/openmsx-about-release-testing-help-wanted
			// lowering it to 7 cycles seems fine. TODO needs more
			// investigation. (Just guessing) possibly there are
			// other variables that influence the exact timing (7
			// vs 8 cycles).
			pendingCpuAccess = true;
			auto delta = isMSX1VDP() ? VDPAccessSlots::DELTA_28
						 : VDPAccessSlots::DELTA_16;
			syncCpuVramAccess.setSyncPoint(getAccessSlot(time, delta));
		}
	}
}

void VDP::executeCpuVramAccess(EmuTime::param time)
{
	int addr = (controlRegs[14] << 14) | vramPointer;
	if (displayMode.isPlanar()) {
		// note: also extended VRAM is interleaved,
		//       because there is only 64kB it's interleaved
		//       with itself (every byte repeated twice)
		addr = ((addr << 16) | (addr >> 1)) & 0x1FFFF;
	}

	bool doAccess;
	if (likely(!cpuExtendedVram)) {
		doAccess = true;
	} else if (likely(vram->getSize() == 192 * 1024)) {
		addr = 0x20000 | (addr & 0xFFFF);
		doAccess = true;
	} else {
		doAccess = false;
	}
	if (doAccess) {
		if (cpuVramReqIsRead) {
			cpuVramData = vram->cpuRead(addr, time);
		} else {
			vram->cpuWrite(addr, cpuVramData, time);
		}
	} else {
		if (cpuVramReqIsRead) {
			cpuVramData = 0xFF;
		} else {
			// nothing
		}
	}

	vramPointer = (vramPointer + 1) & 0x3FFF;
	if (vramPointer == 0 && displayMode.isV9938Mode()) {
		// In MSX2 video modes, pointer range is 128K.
		controlRegs[14] = (controlRegs[14] + 1) & 0x07;
	}
}

EmuTime VDP::getAccessSlot(EmuTime::param time, VDPAccessSlots::Delta delta) const
{
	return VDPAccessSlots::getAccessSlot(
		getFrameStartTime(), time, delta, *this);
}

VDPAccessSlots::Calculator VDP::getAccessSlotCalculator(
	EmuTime::param time, EmuTime::param limit) const
{
	return VDPAccessSlots::getCalculator(
		getFrameStartTime(), time, limit, *this);
}

byte VDP::peekStatusReg(byte reg, EmuTime::param time) const
{
	switch (reg) {
	case 0:
		spriteChecker->sync(time);
		return statusReg0;
	case 1:
		if (controlRegs[0] & 0x10) { // line int enabled
			return statusReg1 | (irqHorizontal.getState() ? 1:0);
		} else { // line int disabled
			// FH goes up at the start of the right border of IL and
			// goes down at the start of the next left border.
			// TODO: Precalc matchLength?
			int afterMatch =
			       getTicksThisFrame(time) - horizontalScanOffset;
			if (afterMatch < 0) {
				afterMatch += getTicksPerFrame();
				// afterMatch can still be negative at this
				// point, see scheduleHScan()
			}
			int matchLength = (displayMode.isTextMode() ? 87 : 59)
			                  + 27 + 100 + 102;
			return statusReg1 |
			       (0 <= afterMatch && afterMatch < matchLength);
		}
	case 2: {
		// TODO: Once VDP keeps display/blanking state, keeping
		//       VR is probably part of that, so use it.
		//       --> Is isDisplayArea actually !VR?
		int ticksThisFrame = getTicksThisFrame(time);
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
		return byte(spriteChecker->getCollisionX(time));
	case 4:
		return byte(spriteChecker->getCollisionX(time) >> 8) | 0xFE;
	case 5:
		return byte(spriteChecker->getCollisionY(time));
	case 6:
		return byte(spriteChecker->getCollisionY(time) >> 8) | 0xFC;
	case 7:
		return cmdEngine->readColor(time);
	case 8:
		return byte(cmdEngine->getBorderX(time));
	case 9:
		return byte(cmdEngine->getBorderX(time) >> 8) | 0xFE;
	default: // non-existent status register
		return 0xFF;
	}
}

byte VDP::readStatusReg(byte reg, EmuTime::param time)
{
	byte ret = peekStatusReg(reg, time);
	switch (reg) {
	case 0:
		spriteChecker->resetStatus();
		statusReg0 &= ~0x80;
		irqVertical.reset();
		break;
	case 1:
		if (controlRegs[0] & 0x10) { // line int enabled
			irqHorizontal.reset();
		}
		break;
	case 5:
		spriteChecker->resetCollision();
		break;
	case 7:
		cmdEngine->resetColor();
		break;
	}
	return ret;
}

byte VDP::readIO(word port, EmuTime::param time)
{
	assert(isInsideFrame(time));

	registerDataStored = false; // Abort any port #1 writes in progress.

	switch (port & (isMSX1VDP() ? 0x01 : 0x03)) {
	case 0: // VRAM data read
		return vramRead(time);
	case 1: // Status register read
		// Calculate status register contents.
		return readStatusReg(controlRegs[15], time);
	case 2:
	case 3:
		return 0xFF;
	default:
		UNREACHABLE; return 0xFF;
	}
}

byte VDP::peekIO(word /*port*/, EmuTime::param /*time*/) const
{
	// TODO not implemented
	return 0xFF;
}

void VDP::changeRegister(byte reg, byte val, EmuTime::param time)
{
	if (reg >= 32) {
		// MXC belongs to CPU interface;
		// other bits in this register belong to command engine.
		if (reg == 45) {
			cpuExtendedVram = (val & 0x40) != 0;
		}
		// Pass command register writes to command engine.
		if (reg < 47) {
			cmdEngine->setCmdReg(reg - 32, val, time);
		}
		return;
	}

	// Make sure only bits that actually exist are written.
	val &= controlValueMasks[reg];
	// Determine the difference between new and old value.
	byte change = val ^ controlRegs[reg];

	// Register 13 is special because writing it resets blinking state,
	// even if the value in the register doesn't change.
	if (reg == 13) {
		// Switch to ON state unless ON period is zero.
		if (blinkState == ((val & 0xF0) == 0)) {
			renderer->updateBlinkState(!blinkState, time);
			blinkState = !blinkState;
		}

		if ((val & 0xF0) && (val & 0x0F)) {
			// Alternating colors, start with ON.
			blinkCount = (val >> 4) * 10;
		} else {
			// Stable color.
			blinkCount = 0;
		}
	}

	if (!change) return;

	// Perform additional tasks before new value becomes active.
	switch (reg) {
	case 0:
		if (change & DisplayMode::REG0_MASK) {
			syncAtNextLine(syncSetMode, time);
		}
		break;
	case 1:
		if (change & 0x03) {
			// Update sprites on size and mag changes.
			spriteChecker->updateSpriteSizeMag(val, time);
		}
		// TODO: Reset vertical IRQ if IE0 is reset?
		if (change & DisplayMode::REG1_MASK) {
			syncAtNextLine(syncSetMode, time);
		}
		if (change & 0x40) {
			syncAtNextLine(syncSetBlank, time);
		}
		break;
	case 2: {
		int base = (val << 10) | ~(~0u << 10);
		// TODO:
		// I reverted this fix.
		// Although the code is correct, there is also a counterpart in the
		// renderer that must be updated. I'm too tired now to find it.
		// Since name table checking is currently disabled anyway, keeping the
		// old code does not hurt.
		// Eventually this line should be re-enabled.
		/*
		if (displayMode.isPlanar()) {
			base = ((base << 16) | (base >> 1)) & 0x1FFFF;
		}
		*/
		renderer->updateNameBase(base, time);
		break;
	}
	case 7:
		if (getDisplayMode().getByte() != DisplayMode::GRAPHIC7) {
			if (change & 0xF0) {
				renderer->updateForegroundColor(val >> 4, time);
			}
			if (change & 0x0F) {
				renderer->updateBackgroundColor(val & 0x0F, time);
			}
		} else {
			renderer->updateBackgroundColor(val, time);
		}
		break;
	case 8:
		if (change & 0x20) {
			renderer->updateTransparency((val & 0x20) == 0, time);
		}
		if (change & 0x02) {
			vram->updateSpritesEnabled((val & 0x02) == 0, time);
		}
		if (change & 0x08) {
			vram->updateVRMode((val & 0x08) != 0, time);
		}
		break;
	case 12:
		if (change & 0xF0) {
			renderer->updateBlinkForegroundColor(val >> 4, time);
		}
		if (change & 0x0F) {
			renderer->updateBlinkBackgroundColor(val & 0x0F, time);
		}
		break;
	case 16:
		// Any half-finished palette loads are aborted.
		paletteDataStored = false;
		break;
	case 18:
		if (change & 0x0F) {
			syncAtNextLine(syncHorAdjust, time);
		}
		break;
	case 23:
		spriteChecker->updateVerticalScroll(val, time);
		renderer->updateVerticalScroll(val, time);
		break;
	case 25:
		if (change & DisplayMode::REG25_MASK) {
			updateDisplayMode(getDisplayMode().updateReg25(val),
			                  time);
		}
		if (change & 0x08) {
			syncAtNextLine(syncHorAdjust, time);
		}
		if (change & 0x02) {
			renderer->updateBorderMask((val & 0x02) != 0, time);
		}
		if (change & 0x01) {
			renderer->updateMultiPage((val & 0x01) != 0, time);
		}
		break;
	case 26:
		renderer->updateHorizontalScrollHigh(val, time);
		break;
	case 27:
		renderer->updateHorizontalScrollLow(val, time);
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
			if (val & 0x20) {
				// This behaviour is important. Without it,
				// the intro music in 'Andonis' is way too slow
				// and the intro logo of 'Zanac' is corrupted.
				if (statusReg0 & 0x80) {
					irqVertical.set();
				}
			} else {
				irqVertical.reset();
			}
		}
		if ((change & 0x80) && isVDPwithVRAMremapping()) {
			// confirmed: VRAM remapping only happens on TMS99xx
			// see VDPVRAM for details on the remapping itself
			vram->change4k8kMapping((val & 0x80) != 0);
		}
		break;
	case 2:
		updateNameBase(time);
		break;
	case 3:
	case 10:
		updateColorBase(time);
		if (vdpHasPatColMirroring()) updatePatternBase(time);
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
		if ((val & 1) && ! warningPrinted) {
			warningPrinted = true;
			getCliComm().printWarning
				("The running MSX software has set bit 0 of VDP register 9 "
				 "(dot clock direction) to one. In an ordinary MSX, "
				 "the screen would go black and the CPU would stop running.");
			// TODO: Emulate such behaviour.
		}
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
	case 25:
		if (change & 0x01) {
			updateNameBase(time);
		}
		break;
	}
}

void VDP::syncAtNextLine(SyncBase& type, EmuTime::param time)
{
	int line = getTicksThisFrame(time) / TICKS_PER_LINE;
	int ticks = (line + 1) * TICKS_PER_LINE;
	EmuTime nextTime = frameStartTime + ticks;
	type.setSyncPoint(nextTime);
}

void VDP::updateNameBase(EmuTime::param time)
{
	int base = (controlRegs[2] << 10) | ~(~0u << 10);
	// TODO:
	// I reverted this fix.
	// Although the code is correct, there is also a counterpart in the
	// renderer that must be updated. I'm too tired now to find it.
	// Since name table checking is currently disabled anyway, keeping the
	// old code does not hurt.
	// Eventually this line should be re-enabled.
	/*
	if (displayMode.isPlanar()) {
		base = ((base << 16) | (base >> 1)) & 0x1FFFF;
	}
	*/
	int indexMask =
		  displayMode.isBitmapMode()
		? ~0u << 17 // TODO: Calculate actual value; how to handle planar?
		: ~0u << (displayMode.isTextMode() ? 12 : 10);
	if (controlRegs[25] & 0x01) {
		// Multi page scrolling. The same bit is used in character and
		// (non)planar-bitmap modes.
		// TODO test text modes
		indexMask &= ~0x8000;
	}
	vram->nameTable.setMask(base, indexMask, time);
}

void VDP::updateColorBase(EmuTime::param time)
{
	int base = (controlRegs[10] << 14) | (controlRegs[3] << 6) | ~(~0u << 6);
	renderer->updateColorBase(base, time);
	switch (displayMode.getBase()) {
	case 0x09: // Text 2.
		// TODO: Enable this only if dual color is actually active.
		vram->colorTable.setMask(base, ~0u << 9, time);
		break;
	case 0x00: // Graphic 1.
		vram->colorTable.setMask(base, ~0u << 6, time);
		break;
	case 0x04: // Graphic 2.
		vram->colorTable.setMask(base | (vdpLacksMirroring() ? 0x1800 : 0), ~0u << 13, time);
		break;
	case 0x08: // Graphic 3.
		vram->colorTable.setMask(base, ~0u << 13, time);
		break;
	default:
		// Other display modes do not use a color table.
		vram->colorTable.disable(time);
	}
}

void VDP::updatePatternBase(EmuTime::param time)
{
	int base = (controlRegs[4] << 11) | ~(~0u << 11);
	renderer->updatePatternBase(base, time);
	switch (displayMode.getBase()) {
	case 0x01: // Text 1.
	case 0x05: // Text 1 Q.
	case 0x09: // Text 2.
	case 0x00: // Graphic 1.
	case 0x02: // Multicolor.
	case 0x06: // Multicolor Q.
		vram->patternTable.setMask(base, ~0u << 11, time);
		break;
	case 0x04: // Graphic 2.
		if (vdpHasPatColMirroring()) {
			// TMS99XX has weird pattern table behavior: some
			// bits of the color-base register leak into the
			// pattern-base. See also:
			//   http://www.youtube.com/watch?v=XJljSJqzDR0
			base =  (controlRegs[4]         << 11)
			     | ((controlRegs[3] & 0x1f) <<  6)
			     | ~(~0u << 6);
		}
		vram->patternTable.setMask(base | (vdpLacksMirroring() ? 0x1800 : 0), ~0u << 13, time);
		break;
	case 0x08: // Graphic 3.
		vram->patternTable.setMask(base, ~0u << 13, time);
		break;
	default:
		// Other display modes do not use a pattern table.
		vram->patternTable.disable(time);
	}
}

void VDP::updateSpriteAttributeBase(EmuTime::param time)
{
	int mode = displayMode.getSpriteMode(isMSX1VDP());
	if (mode == 0) {
		vram->spriteAttribTable.disable(time);
		return;
	}
	int baseMask = (controlRegs[11] << 15) | (controlRegs[5] << 7) | ~(~0u << 7);
	int indexMask = mode == 1 ? ~0u << 7 : ~0u << 10;
	if (displayMode.isPlanar()) {
		baseMask = ((baseMask << 16) | (baseMask >> 1)) & 0x1FFFF;
		indexMask = ((indexMask << 16) | ~(1 << 16)) & (indexMask >> 1);
	}
	vram->spriteAttribTable.setMask(baseMask, indexMask, time);
}

void VDP::updateSpritePatternBase(EmuTime::param time)
{
	if (displayMode.getSpriteMode(isMSX1VDP()) == 0) {
		vram->spritePatternTable.disable(time);
		return;
	}
	int baseMask = (controlRegs[6] << 11) | ~(~0u << 11);
	int indexMask = ~0u << 11;
	if (displayMode.isPlanar()) {
		baseMask = ((baseMask << 16) | (baseMask >> 1)) & 0x1FFFF;
		indexMask = ((indexMask << 16) | ~(1 << 16)) & (indexMask >> 1);
	}
	vram->spritePatternTable.setMask(baseMask, indexMask, time);
}

void VDP::updateDisplayMode(DisplayMode newMode, EmuTime::param time)
{
	// Synchronise subsystems.
	vram->updateDisplayMode(newMode, time);

	// TODO: Is this a useful optimisation, or doesn't it help
	//       in practice?
	// What aspects have changed:
	// Switched from planar to nonplanar or vice versa.
	bool planarChange =
		newMode.isPlanar() != displayMode.isPlanar();
	// Sprite mode changed.
	bool msx1 = isMSX1VDP();
	bool spriteModeChange =
		newMode.getSpriteMode(msx1) != displayMode.getSpriteMode(msx1);

	// Commit the new display mode.
	displayMode = newMode;

	// Speed up performance of bitmap/character mode splits:
	// leave last used character mode active.
	// TODO: Disable it if not used for some time.
	if (!displayMode.isBitmapMode()) {
		updateColorBase(time);
		updatePatternBase(time);
	}
	if (planarChange || spriteModeChange) {
		updateSpritePatternBase(time);
		updateSpriteAttributeBase(time);
	}
	updateNameBase(time);

	// To be extremely accurate, reschedule hscan when changing
	// from/to text mode. Text mode has different border width,
	// which affects the moment hscan occurs.
	// TODO: Why didn't I implement this yet?
	//       It's one line of code and overhead is not huge either.
}

void VDP::update(const Setting& setting)
{
	assert((&setting == &cmdTiming) ||
	       (&setting == &tooFastAccess));
	(void)setting;
	brokenCmdTiming    = cmdTiming    .getEnum();
	allowTooFastAccess = tooFastAccess.getEnum();

	if (unlikely(allowTooFastAccess && pendingCpuAccess)) {
		// in allowTooFastAccess-mode, don't schedule CPU-VRAM access
		syncCpuVramAccess.removeSyncPoint();
		pendingCpuAccess = false;
		executeCpuVramAccess(getCurrentTime());
	}
}

// RegDebug

VDP::RegDebug::RegDebug(VDP& vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(),
	                   vdp_.getName() + " regs", "VDP registers.", 0x40)
{
}

byte VDP::RegDebug::read(unsigned address)
{
	auto& vdp = OUTER(VDP, vdpRegDebug);
	if (address < 0x20) {
		return vdp.controlRegs[address];
	} else if (address < 0x2F) {
		return vdp.cmdEngine->peekCmdReg(address - 0x20);
	} else {
		return 0xFF;
	}
}

void VDP::RegDebug::write(unsigned address, byte value, EmuTime::param time)
{
	auto& vdp = OUTER(VDP, vdpRegDebug);
	vdp.changeRegister(address, value, time);
}


// StatusRegDebug

VDP::StatusRegDebug::StatusRegDebug(VDP& vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(),
	                   vdp_.getName() + " status regs", "VDP status registers.", 0x10)
{
}

byte VDP::StatusRegDebug::read(unsigned address, EmuTime::param time)
{
	auto& vdp = OUTER(VDP, vdpStatusRegDebug);
	return vdp.peekStatusReg(address, time);
}


// PaletteDebug

VDP::PaletteDebug::PaletteDebug(VDP& vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(),
	                   vdp_.getName() + " palette", "V99x8 palette (RBG format)", 0x20)
{
}

byte VDP::PaletteDebug::read(unsigned address)
{
	auto& vdp = OUTER(VDP, vdpPaletteDebug);
	word grb = vdp.getPalette(address / 2);
	return (address & 1) ? (grb >> 8) : (grb & 0xff);
}

void VDP::PaletteDebug::write(unsigned address, byte value, EmuTime::param time)
{
	auto& vdp = OUTER(VDP, vdpPaletteDebug);
	int index = address / 2;
	word grb = vdp.getPalette(index);
	grb = (address & 1)
	    ? (grb & 0x0077) | ((value & 0x07) << 8)
	    : (grb & 0x0700) |  (value & 0x77);
	vdp.setPalette(index, grb, time);
}


// class VRAMPointerDebug

VDP::VRAMPointerDebug::VRAMPointerDebug(VDP& vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(), vdp_.getName() == "VDP" ?
			"VRAM pointer" : vdp_.getName() + " VRAM pointer",
			"VDP VRAM pointer (14 lower bits)", 2)
{
}

byte VDP::VRAMPointerDebug::read(unsigned address)
{
	auto& vdp = OUTER(VDP, vramPointerDebug);
	if (address & 1) {
		return vdp.vramPointer >> 8;  // TODO add read/write mode?
	} else {
		return vdp.vramPointer & 0xFF;
	}
}

void VDP::VRAMPointerDebug::write(unsigned address, byte value, EmuTime::param /*time*/)
{
	auto& vdp = OUTER(VDP, vramPointerDebug);
	int& ptr = vdp.vramPointer;
	if (address & 1) {
		ptr = (ptr & 0x00FF) | ((value & 0x3F) << 8);
	} else {
		ptr = (ptr & 0xFF00) | value;
	}
}


// class Info

VDP::Info::Info(VDP& vdp_, const string& name_, string helpText_)
	: InfoTopic(vdp_.getMotherBoard().getMachineInfoCommand(),
		    vdp_.getName() + '_' + name_)
	, vdp(vdp_)
	, helpText(std::move(helpText_))
{
}

void VDP::Info::execute(array_ref<TclObject> /*tokens*/, TclObject& result) const
{
	result.setInt(calc(vdp.getCurrentTime()));
}

string VDP::Info::help(const vector<string>& /*tokens*/) const
{
	return helpText;
}


// class FrameCountInfo

VDP::FrameCountInfo::FrameCountInfo(VDP& vdp_)
	: Info(vdp_, "frame_count",
	       "The current frame number, starts counting at 0 "
	       "when MSX is powered up or reset.")
{
}

int VDP::FrameCountInfo::calc(const EmuTime& /*time*/) const
{
	return vdp.frameCount;
}


// class CycleInFrameInfo

VDP::CycleInFrameInfo::CycleInFrameInfo(VDP& vdp_)
	: Info(vdp_, "cycle_in_frame",
	       "The number of VDP cycles since the beginning of "
	       "the current frame. The VDP runs at 6 times the Z80 "
	       "clock frequency, so at approximately 21.5MHz.")
{
}

int VDP::CycleInFrameInfo::calc(const EmuTime& time) const
{
	return vdp.getTicksThisFrame(time);
}


// class LineInFrameInfo

VDP::LineInFrameInfo::LineInFrameInfo(VDP& vdp_)
	: Info(vdp_, "line_in_frame",
	       "The absolute line number since the beginning of "
	       "the current frame. Goes from 0 till 262 (NTSC) or "
	       "313 (PAL). Note that this number includes the "
	       "border lines, use 'msx_y_pos' to get MSX "
	       "coordinates.")
{
}

int VDP::LineInFrameInfo::calc(const EmuTime& time) const
{
	return vdp.getTicksThisFrame(time) / VDP::TICKS_PER_LINE;
}


// class CycleInLineInfo

VDP::CycleInLineInfo::CycleInLineInfo(VDP& vdp_)
	: Info(vdp_, "cycle_in_line",
	       "The number of VDP cycles since the beginning of "
	       "the current line. See also 'cycle_in_frame'."
	       "Note that this includes the cycles in the border, "
	       "use 'msx_x256_pos' or 'msx_x512_pos' to get MSX "
	       "coordinates.")
{
}

int VDP::CycleInLineInfo::calc(const EmuTime& time) const
{
	return vdp.getTicksThisFrame(time) % VDP::TICKS_PER_LINE;
}


// class MsxYPosInfo

VDP::MsxYPosInfo::MsxYPosInfo(VDP& vdp_)
	: Info(vdp_, "msx_y_pos",
	       "Similar to 'line_in_frame', but expressed in MSX "
	       "coordinates. So lines in the top border have "
	       "negative coordinates, lines in the bottom border "
	       "have coordinates bigger or equal to 192 or 212.")
{
}

int VDP::MsxYPosInfo::calc(const EmuTime& time) const
{
	return (vdp.getTicksThisFrame(time) / VDP::TICKS_PER_LINE) -
		vdp.getLineZero();
}


// class MsxX256PosInfo

VDP::MsxX256PosInfo::MsxX256PosInfo(VDP& vdp_)
	: Info(vdp_, "msx_x256_pos",
	       "Similar to 'cycle_in_frame', but expressed in MSX "
	       "coordinates. So a position in the left border has "
	       "a negative coordinate and a position in the right "
	       "border has a coordinated bigger or equal to 256. "
	       "See also 'msx_x512_pos'.")
{
}

int VDP::MsxX256PosInfo::calc(const EmuTime& time) const
{
	return ((vdp.getTicksThisFrame(time) % VDP::TICKS_PER_LINE) -
		 vdp.getLeftSprites()) / 4;
}


// class MsxX512PosInfo

VDP::MsxX512PosInfo::MsxX512PosInfo(VDP& vdp_)
	: Info(vdp_, "msx_x512_pos",
	       "Similar to 'cycle_in_frame', but expressed in "
	       "'narrow' (screen 7) MSX coordinates. So a position "
	       "in the left border has a negative coordinate and "
	       "a position in the right border has a coordinated "
	       "bigger or equal to 512. See also 'msx_x256_pos'.")
{
}

int VDP::MsxX512PosInfo::calc(const EmuTime& time) const
{
	return ((vdp.getTicksThisFrame(time) % VDP::TICKS_PER_LINE) -
		 vdp.getLeftSprites()) / 2;
}


// version 1: initial version
// version 2: added frameCount
// version 3: removed verticalAdjust
// version 4: removed lineZero
// version 5: replace readAhead->cpuVramData, added cpuVramReqIsRead
// version 6: added cpuVramReqAddr to solve too_fast_vram_access issue
// version 7: removed cpuVramReqAddr again, fixed issue in a different way
// version 8: removed 'userData' from Schedulable
template<typename Archive>
void VDP::serialize(Archive& ar, unsigned serVersion)
{
	ar.template serializeBase<MSXDevice>(*this);

	if (ar.versionAtLeast(serVersion, 8)) {
		ar.serialize("syncVSync",         syncVSync);
		ar.serialize("syncDisplayStart",  syncDisplayStart);
		ar.serialize("syncVScan",         syncVScan);
		ar.serialize("syncHScan",         syncHScan);
		ar.serialize("syncHorAdjust",     syncHorAdjust);
		ar.serialize("syncSetMode",       syncSetMode);
		ar.serialize("syncSetBlank",      syncSetBlank);
		ar.serialize("syncCpuVramAccess", syncCpuVramAccess);
	} else {
		Schedulable::restoreOld(ar,
			{&syncVSync, &syncDisplayStart, &syncVScan,
			 &syncHScan, &syncHorAdjust, &syncSetMode,
			 &syncSetBlank, &syncCpuVramAccess});
	}

	// not serialized
	//    std::unique_ptr<Renderer> renderer;
	//    VdpVersion version;
	//    int controlRegMask;
	//    byte controlValueMasks[32];
	//    bool warningPrinted;

	ar.serialize("irqVertical", irqVertical);
	ar.serialize("irqHorizontal", irqHorizontal);
	ar.serialize("frameStartTime", frameStartTime);
	ar.serialize("displayStartSyncTime", displayStartSyncTime);
	ar.serialize("vScanSyncTime", vScanSyncTime);
	ar.serialize("hScanSyncTime", hScanSyncTime);
	ar.serialize("displayStart", displayStart);
	ar.serialize("horizontalScanOffset", horizontalScanOffset);
	ar.serialize("horizontalAdjust", horizontalAdjust);
	ar.serialize("registers", controlRegs);
	ar.serialize("blinkCount", blinkCount);
	ar.serialize("vramPointer", vramPointer);
	ar.serialize("palette", palette);
	ar.serialize("isDisplayArea", isDisplayArea);
	ar.serialize("palTiming", palTiming);
	ar.serialize("interlaced", interlaced);
	ar.serialize("statusReg0", statusReg0);
	ar.serialize("statusReg1", statusReg1);
	ar.serialize("statusReg2", statusReg2);
	ar.serialize("blinkState", blinkState);
	ar.serialize("dataLatch", dataLatch);
	ar.serialize("registerDataStored", registerDataStored);
	ar.serialize("paletteDataStored", paletteDataStored);
	if (ar.versionAtLeast(serVersion, 5)) {
		ar.serialize("cpuVramData", cpuVramData);
		ar.serialize("cpuVramReqIsRead", cpuVramReqIsRead);
	} else {
		ar.serialize("readAhead", cpuVramData);
	}
	ar.serialize("cpuExtendedVram", cpuExtendedVram);
	ar.serialize("displayEnabled", displayEnabled);
	byte mode = displayMode.getByte();
	ar.serialize("displayMode", mode);
	displayMode.setByte(mode);

	ar.serialize("cmdEngine", *cmdEngine);
	ar.serialize("spriteChecker", *spriteChecker); // must come after displayMode
	ar.serialize("vram", *vram); // must come after controlRegs and after spriteChecker
	if (ar.isLoader()) {
		pendingCpuAccess = syncCpuVramAccess.pendingSyncPoint();
		update(tooFastAccess);
	}

	if (ar.versionAtLeast(serVersion, 2)) {
		ar.serialize("frameCount", frameCount);
	} else {
		assert(ar.isLoader());
		// We could estimate the frameCount (assume framerate was
		// constant the whole time). But I think it's better to have
		// an obviously wrong value than an almost correct value.
		frameCount = 0;
	}

	// externalVideo does not need serializing. It is set on load by the
	// external video source (e.g. PioneerLDControl).
	//
	// TODO should superimposing be serialized? It cannot be recalculated
	// from register values (it depends on the register values at the start
	// of this frame). But it will be correct at the start of the next
	// frame. Probably good enough.

	if (ar.isLoader()) {
		renderer->reInit();
	}
}
INSTANTIATE_SERIALIZE_METHODS(VDP);
REGISTER_MSXDEVICE(VDP, "VDP");

} // namespace openmsx
