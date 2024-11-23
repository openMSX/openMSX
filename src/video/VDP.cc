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

#include "Display.hh"
#include "RenderSettings.hh"
#include "Renderer.hh"
#include "RendererFactory.hh"
#include "SpriteChecker.hh"
#include "VDPCmdEngine.hh"
#include "VDPVRAM.hh"

#include "EnumSetting.hh"
#include "HardwareConfig.hh"
#include "MSXCPU.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "TclObject.hh"
#include "serialize_core.hh"

#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "unreachable.hh"

#include <cassert>
#include <memory>

namespace openmsx {

static byte getDelayCycles(const XMLElement& devices) {
	byte cycles = 0;
	if (const auto* t9769Dev = devices.findChild("T9769")) {
		if (t9769Dev->getChildData("subtype") == "C") {
			cycles = 1;
		} else {
			cycles = 2;
		}
	} else if (devices.findChild("S1990")) {
		// this case is purely there for backwards compatibility for
		// turboR configs which do not have the T9769 tag yet.
		cycles = 1;
	}
	return cycles;
}

VDP::VDP(const DeviceConfig& config)
	: MSXDevice(config)
	, syncVSync(*this)
	, syncDisplayStart(*this)
	, syncVScan(*this)
	, syncHScan(*this)
	, syncHorAdjust(*this)
	, syncSetMode(*this)
	, syncSetBlank(*this)
	, syncSetSprites(*this)
	, syncCpuVramAccess(*this)
	, syncCmdDone(*this)
	, display(getReactor().getDisplay())
	, cmdTiming    (display.getRenderSettings().getCmdTimingSetting())
	, tooFastAccess(display.getRenderSettings().getTooFastAccessSetting())
	, vdpRegDebug      (*this)
	, vdpStatusRegDebug(*this)
	, vdpPaletteDebug  (*this)
	, vramPointerDebug (*this)
	, registerLatchStatusDebug(*this)
	, vramAccessStatusDebug(*this)
	, paletteLatchStatusDebug(*this)
	, dataLatchDebug   (*this)
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
		"Tcl proc called when the VRAM is read or written too fast",
		"",
		Setting::Save::YES)
	, dotClockDirectionCallback(
		getCommandController(),
		getName() + ".dot_clock_direction_callback",
		"Tcl proc called when DLCLK is set as input",
		"default_dot_clock_direction_callback",
		Setting::Save::YES)
	, cpu(getCPU()) // used frequently, so cache it
	, fixedVDPIOdelayCycles(getDelayCycles(getMotherBoard().getMachineConfig()->getConfig().getChild("devices")))
{
	// Current general defaults for saturation:
	// - Any MSX with a TMS9x18 VDP: SatPr=SatPb=100%
	// - Other machines with a TMS9x2x VDP and RGB output:
	//   SatPr=SatPr=100%, until someone reports a better value
	// - Other machines with a TMS9x2x VDP and CVBS only output:
	//   SatPr=SatPb=54%, until someone reports a better value
	// At this point we don't know anything about the connector here, so
	// only the first point can be implemented. The default default is
	// still 54, which is very similar to the old palette implementation

	int defaultSaturation = 54;

	const auto& versionString = config.getChildData("version");
	if (versionString == "TMS99X8A") version = TMS99X8A;
	else if (versionString == "TMS9918A") {
		version = TMS99X8A;
		defaultSaturation = 100;
	} else if (versionString == "TMS9928A") version = TMS99X8A;
	else if (versionString == "T6950PAL") version = T6950PAL;
	else if (versionString == "T6950NTSC") version = T6950NTSC;
	else if (versionString == "T7937APAL") version = T7937APAL;
	else if (versionString == "T7937ANTSC") version = T7937ANTSC;
	else if (versionString == "TMS91X8") version = TMS91X8;
	else if (versionString == "TMS9118") {
		version = TMS91X8;
		defaultSaturation = 100;
	} else if (versionString == "TMS9128") version = TMS91X8;
	else if (versionString == "TMS9929A") version = TMS9929A;
	else if (versionString == "TMS9129") version = TMS9129;
	else if (versionString == "V9938") version = V9938;
	else if (versionString == "V9958") version = V9958;
	else if (versionString == "YM2220PAL") version = YM2220PAL;
	else if (versionString == "YM2220NTSC") version = YM2220NTSC;
	else throw MSXException("Unknown VDP version \"", versionString, '"');

	// saturation parameters only make sense when using TMS VDPs
	if (!versionString.starts_with("TMS") &&
	    (config.findChild("saturationPr") || config.findChild("saturationPb") || config.findChild("saturation"))) {
		throw MSXException("Specifying saturation parameters only makes sense for TMS VDPs");
	}

	auto getPercentage = [&](std::string_view name, std::string_view extra, int defaultValue) {
		int result = config.getChildDataAsInt(name, defaultValue);
		if ((result < 0) || (result > 100)) {
			throw MSXException(
				"Saturation percentage ", extra, "is not in range 0..100: ", result);
		}
		return result;
	};
	int saturation = getPercentage("saturation", "", defaultSaturation);
	saturationPr   = getPercentage("saturationPr", "for Pr component ", saturation);
	saturationPb   = getPercentage("saturationPb", "for Pb component ", saturation);

	// Set up control register availability.
	static constexpr std::array<byte, 32> VALUE_MASKS_MSX1 = {
		0x03, 0xFB, 0x0F, 0xFF, 0x07, 0x7F, 0x07, 0xFF  // 00..07
	};
	static constexpr std::array<byte, 32> VALUE_MASKS_MSX2 = {
		0x7E, 0x7F, 0x7F, 0xFF, 0x3F, 0xFF, 0x3F, 0xFF, // 00..07
		0xFB, 0xBF, 0x07, 0x03, 0xFF, 0xFF, 0x07, 0x0F, // 08..15
		0x0F, 0xBF, 0xFF, 0xFF, 0x3F, 0x3F, 0x3F, 0xFF, // 16..23
		0,    0,    0,    0,    0,    0,    0,    0,    // 24..31
	};
	controlRegMask = isMSX1VDP() ? 0x07 : 0x3F;
	controlValueMasks = isMSX1VDP() ? VALUE_MASKS_MSX1 : VALUE_MASKS_MSX2;
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
		(isMSX1VDP() ? 16 : config.getChildDataAsInt("vram", 0));
	if (vramSize != one_of(16u, 64u, 128u, 192u)) {
		throw MSXException(
			"VRAM size of ", vramSize, "kB is not supported!");
	}
	vram = std::make_unique<VDPVRAM>(*this, vramSize * 1024, time);

	// Create sprite checker.
	auto& renderSettings = display.getRenderSettings();
	spriteChecker = std::make_unique<SpriteChecker>(*this, renderSettings, time);
	vram->setSpriteChecker(spriteChecker.get());

	// Create command engine.
	cmdEngine = std::make_unique<VDPCmdEngine>(*this, getCommandController());
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

void VDP::preVideoSystemChange() noexcept
{
	renderer.reset();
}

void VDP::postVideoSystemChange() noexcept
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
	ranges::fill(controlRegs, 0);
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
	cpuVramReqIsRead = false; // avoid UMR
	dataLatch = 0;
	cpuExtendedVram = false;
	registerDataStored = false;
	writeAccess = false;
	paletteDataStored = false;
	blinkState = false;
	blinkCount = 0;
	horizontalAdjust = 7;

	// TODO: Real VDP probably resets timing as well.
	isDisplayArea = false;
	displayEnabled = false;
	spriteEnabled = true;
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
	const std::array<uint16_t, 16> V9938_PALETTE = {
		0x000, 0x000, 0x611, 0x733, 0x117, 0x327, 0x151, 0x627,
		0x171, 0x373, 0x661, 0x664, 0x411, 0x265, 0x555, 0x777
	};
	// Init the palette.
	palette = V9938_PALETTE;
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
	syncSetSprites   .removeSyncPoint();
	syncCpuVramAccess.removeSyncPoint();
	syncCmdDone      .removeSyncPoint();
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

	if (isFastBlinkEnabled()) {
		// adjust blinkState and blinkCount for next frame
		auto next = calculateLineBlinkState(getLinesPerFrame());
		blinkState = next.state;
		blinkCount = next.count;
	}

	// Finish the previous frame, because access-slot calculations work within a frame.
	cmdEngine->sync(time);

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
		getCmdBit(),
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

void VDP::execSetSprites(EmuTime::param time)
{
	bool newSpriteEnabled = (controlRegs[8] & 0x02) == 0;
	vram->updateSpritesEnabled(newSpriteEnabled, time);
	spriteEnabled = newSpriteEnabled;
}

void VDP::execCpuVramAccess(EmuTime::param time)
{
	assert(!allowTooFastAccess);
	pendingCpuAccess = false;
	executeCpuVramAccess(time);
}

void VDP::execSyncCmdDone(EmuTime::param time)
{
	cmdEngine->sync(time);
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
	int lineZero =
		// sync + top erase:
		3 + 13 +
		// top border:
		(palTiming ? 36 : 9) +
		(controlRegs[9] & 0x80 ? 0 : 10) +
		getVerticalAdjust(); // 0..15
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
	// Remove pending VSCAN sync point, if any.
	if (vScanSyncTime > time) {
		syncVScan.removeSyncPoint();
		//cerr << "removing predicted VSCAN sync point\n";
	}

	// Calculate moment in time display end occurs.
	vScanSyncTime = frameStartTime +
	                (displayStart + getNumberOfLines() * TICKS_PER_LINE);

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
	if (int ticksPerFrame = getTicksPerFrame();
	    horizontalScanOffset >= ticksPerFrame) {
		horizontalScanOffset -= ticksPerFrame;

		// Time at which the internal VDP display line counter is reset,
		// expressed in ticks after vsync.
		// I would expect the counter to reset at line 16 (for neutral
		// set-adjust), but measurements on NMS8250 show it is one line
		// earlier. I'm not sure whether the actual counter reset
		// happens on line 15 or whether the VDP timing may be one line
		// off for some reason.
		// TODO: This is just an assumption, more measurements on real MSX
		//       are necessary to verify there is really such a thing and
		//       if so, that the value is accurate.
		// Note: see this bug report for some measurements on a real machine:
		//   https://github.com/openMSX/openMSX/issues/1106
		int lineCountResetTicks = (8 + getVerticalAdjust()) * TICKS_PER_LINE;

		// Display line counter is reset at the start of the top border.
		// Any HSCAN that has a higher line number never occurs.
		if (horizontalScanOffset >= lineCountResetTicks) {
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

	// Toggle E/O.
	// Actually this should occur half a line earlier,
	// but for now this is accurate enough.
	statusReg2 ^= 0x02;

	// Settings which are fixed at start of frame.
	// Not sure this is how real MSX does it, but close enough for now.
	// TODO: Interlace is effectuated in border height, according to
	//       the data book. Exactly when is the fixation point?
	palTiming = (controlRegs[9] & 0x02) != 0;
	interlaced = !isFastBlinkEnabled() && ((controlRegs[9] & 0x08) != 0);

	// Blinking.
	if ((blinkCount != 0) && !isFastBlinkEnabled()) { // counter active?
		blinkCount--;
		if (blinkCount == 0) {
			renderer->updateBlinkState(!blinkState, time);
			blinkState = !blinkState;
			blinkCount = (blinkState
				? controlRegs[13] >> 4 : controlRegs[13] & 0x0F) * 10;
		}
	}

	// TODO: Presumably this is done here
	// Note that if superimposing is enabled but no external video
	// signal is provided then the VDP stops producing a signal
	// (at least on an MSX1, VDP(0)=1 produces "signal lost" on my
	// monitor)
	if (const RawFrame* newSuperimposing = (controlRegs[0] & 1) ? externalVideo : nullptr;
	    superimposing != newSuperimposing) {
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

void VDP::writeIO(word port, byte value, EmuTime::param time_)
{
	EmuTime time = time_;
	// This is the (fixed) delay from
	// https://github.com/openMSX/openMSX/issues/563 and
	// https://github.com/openMSX/openMSX/issues/989
	// It seems to originate from the T9769x and for x=C the delay is 1
	// cycle and for other x it seems the delay is 2 cycles
	if (fixedVDPIOdelayCycles > 0) {
		time = cpu.waitCyclesZ80(time, fixedVDPIOdelayCycles);
	}

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
						time);
				} else {
					// TODO what happens in this case?
					// it's not a register write because
					// that breaks "SNOW26" demo
				}
				if (isMSX1VDP()) {
					// For these VDPs the VRAM pointer is modified when
					// writing to VDP registers. Without this some demos won't
					// run as on real MSX1, e.g. Planet of the Epas, Utopia and
					// Waves 1.2. Thanks to dvik for finding this out.
					// See also below about not using the latch on MSX1.
					// Set read/write address.
					vramPointer = (value << 8 | (vramPointer & 0xFF)) & 0x3FFF;
				}
			} else {
				// Set read/write address.
				writeAccess = value & 0x40;
				vramPointer = (value << 8 | dataLatch) & 0x3FFF;
				if (!(value & 0x40)) {
					// Read ahead.
					(void)vramRead(time);
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
			unsigned index = controlRegs[16];
			word grb = ((value << 8) | dataLatch) & 0x777;
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

std::string_view VDP::getVersionString() const
{
	// Add VDP type from the config. An alternative is to convert the
	// 'version' enum member into some kind of string, but we already
	// parsed that string to become this version enum value. So whatever is
	// in there, it makes sense. So we can just as well return that then.
	const auto* vdpVersionString = getDeviceConfig().findChild("version");
	assert(vdpVersionString);
	return vdpVersionString->getData();
}

void VDP::getExtraDeviceInfo(TclObject& result) const
{
	result.addDictKeyValues("version", getVersionString());
}

byte VDP::peekRegister(unsigned address) const
{
	if (address < 0x20) {
		return controlRegs[address];
	} else if (address < 0x2F) {
		return cmdEngine->peekCmdReg(narrow<byte>(address - 0x20));
	} else {
		return 0xFF;
	}
}

void VDP::setPalette(unsigned index, word grb, EmuTime::param time)
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
	if (pendingCpuAccess) [[unlikely]] {
		// Already scheduled. Do nothing.
		// The old request has been overwritten by the new request!
		assert(!allowTooFastAccess);
		tooFastCallback.execute();
	} else {
		if (allowTooFastAccess) [[unlikely]] {
			// Immediately execute request.
			// In the past, in allowTooFastAccess-mode, we would
			// still schedule the actual access, but process
			// pending requests early when a new one arrives before
			// the old one was handled. Though this would still go
			// wrong because of the delayed update of
			// 'vramPointer'. We could _only_ _partly_ work around
			// that by calculating the actual vram address early
			// (likely not what the real VDP does). But because
			// allowTooFastAccess is anyway an artificial situation
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
			// Update: 8 cycles (Delta::D32) causes corruption in
			// 'Chase HQ', see
			//    http://www.msx.org/forum/msx-talk/openmsx/openmsx-about-release-testing-help-wanted
			// lowering it to 7 cycles seems fine. TODO needs more
			// investigation. (Just guessing) possibly there are
			// other variables that influence the exact timing (7
			// vs 8 cycles).
			pendingCpuAccess = true;
			auto delta = isMSX1VDP() ? VDPAccessSlots::Delta::D28
						 : VDPAccessSlots::Delta::D16;
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

	bool doAccess = [&] {
		if (!cpuExtendedVram) [[likely]] {
			return true;
		} else if (vram->getSize() == 192 * 1024) [[likely]] {
			addr = 0x20000 | (addr & 0xFFFF);
			return true;
		} else {
			return false;
		}
	}();
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
		UNREACHABLE;
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
		// TODO when 'isFastBlinkEnabled()==true' the variables
		// 'blinkState' and 'blinkCount' represent the values at line 0.
		// This implementation is not correct for the partial remaining
		// frame after register 13 got changed.
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
		unsigned base = (val << 10) | ~(~0u << 10);
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
			spriteChecker->updateTransparency((val & 0x20) == 0, time);
		}
		if (change & 0x02) {
			syncAtNextLine(syncSetSprites, time);
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
		if (change & (DisplayMode::REG25_MASK | 0x40)) {
			updateDisplayMode(getDisplayMode().updateReg25(val),
			                  val & 0x40,
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
			dotClockDirectionCallback.execute();
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

void VDP::syncAtNextLine(SyncBase& type, EmuTime::param time) const
{
	// The processing of a new line starts in the middle of the left erase,
	// ~144 cycles after the sync signal. Adjust affects it. See issue #1310.
	int offset = 144 + (horizontalAdjust - 7) * 4;
	int line = (getTicksThisFrame(time) + TICKS_PER_LINE - offset) / TICKS_PER_LINE;
	int ticks = line * TICKS_PER_LINE + offset;
	EmuTime nextTime = frameStartTime + ticks;
	type.setSyncPoint(nextTime);
}

void VDP::updateNameBase(EmuTime::param time)
{
	unsigned base = (controlRegs[2] << 10) | ~(~0u << 10);
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
	unsigned indexMask =
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
	unsigned base = (controlRegs[10] << 14) | (controlRegs[3] << 6) | ~(~0u << 6);
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
	unsigned base = (controlRegs[4] << 11) | ~(~0u << 11);
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
	unsigned baseMask = (controlRegs[11] << 15) | (controlRegs[5] << 7) | ~(~0u << 7);
	unsigned indexMask = mode == 1 ? ~0u << 7 : ~0u << 10;
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
	unsigned baseMask = (controlRegs[6] << 11) | ~(~0u << 11);
	unsigned indexMask = ~0u << 11;
	if (displayMode.isPlanar()) {
		baseMask = ((baseMask << 16) | (baseMask >> 1)) & 0x1FFFF;
		indexMask = ((indexMask << 16) | ~(1 << 16)) & (indexMask >> 1);
	}
	vram->spritePatternTable.setMask(baseMask, indexMask, time);
}

void VDP::updateDisplayMode(DisplayMode newMode, bool cmdBit, EmuTime::param time)
{
	// Synchronize subsystems.
	vram->updateDisplayMode(newMode, cmdBit, time);

	// TODO: Is this a useful optimisation, or doesn't it help
	//       in practice?
	// What aspects have changed:
	// Switched from planar to non-planar or vice versa.
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

void VDP::update(const Setting& setting) noexcept
{
	assert(&setting == one_of(&cmdTiming, &tooFastAccess));
	(void)setting;
	brokenCmdTiming    = cmdTiming    .getEnum();
	allowTooFastAccess = tooFastAccess.getEnum();

	if (allowTooFastAccess && pendingCpuAccess) [[unlikely]] {
		// in allowTooFastAccess-mode, don't schedule CPU-VRAM access
		syncCpuVramAccess.removeSyncPoint();
		pendingCpuAccess = false;
		executeCpuVramAccess(getCurrentTime());
	}
}

/*
 * Roughly measured RGB values in volts.
 * Voltages were in range of 1.12-5.04, and had 2 digits accuracy (it seems
 * minimum difference was 0.04 V).
 * Blue component of color 5 and red component of color 9 were higher than
 * the components for white... There are several methods to handle this...
 * 1) clip to values of white
 * 2) scale all colors by min/max of that component (means white is not 3x 255)
 * 3) scale per color if components for that color are beyond those of white
 * 4) assume the analog values are output by a DA converter, derive the digital
 *    values and scale that to the range 0-255 (thanks to FRS for this idea).
 *    This also results in white not being 3x 255, of course.
 *
 * Method 4 results in the table below and seems the most accurate (so far).
 *
 * Thanks to Tiago Valença and Carlos Mansur for measuring on a T7937A.
 */
static constexpr std::array<std::array<uint8_t, 3>, 16> TOSHIBA_PALETTE = {{
	{   0,   0,   0 },
	{   0,   0,   0 },
	{ 102, 204, 102 },
	{ 136, 238, 136 },
	{  68,  68, 221 },
	{ 119, 119, 255 },
	{ 187,  85,  85 },
	{ 119, 221, 221 },
	{ 221, 102, 102 },
	{ 255, 119, 119 },
	{ 204, 204,  85 },
	{ 238, 238, 136 },
	{  85, 170,  85 },
	{ 187,  85, 187 },
	{ 204, 204, 204 },
	{ 238, 238, 238 },
}};

/*
 * Palette for the YM2220 is a crude approximation based on the fact that the
 * pictures of a Yamaha AX-150 (YM2220) and a Philips NMS-8250 (V9938) have a
 * quite similar appearance. See first post here:
 *
 * https://www.msx.org/forum/msx-talk/hardware/unknown-vdp-yamaha-ym2220?page=3
 */
static constexpr std::array<std::array<uint8_t, 3>, 16> YM2220_PALETTE = {{
	{   0,   0,   0 },
	{   0,   0,   0 },
	{  36, 218,  36 },
	{ 200, 255, 109 },
	{  36,  36, 255 },
	{  72, 109, 255 },
	{ 182,  36,  36 },
	{  72, 218, 255 },
	{ 255,  36,  36 },
	{ 255, 175, 175 },
	{ 230, 230,   0 },
	{ 230, 230, 200 },
	{  36, 195,  36 },
	{ 218,  72, 182 },
	{ 182, 182, 182 },
	{ 255, 255, 255 },
}};

/*
How come the FM-X has a distinct palette while it clearly has a TMS9928 VDP?
Because it has an additional circuit that rework the palette for the same one
used in the Fujitsu FM-7. It's encoded in 3-bit RGB.

This seems to be the 24-bit RGB equivalent to the palette output by the FM-X on
its RGB connector:
*/
static constexpr std::array<std::array<uint8_t, 3>, 16> THREE_BIT_RGB_PALETTE = {{
	{   0,   0,   0 },
	{   0,   0,   0 },
	{   0, 255,   0 },
	{   0, 255,   0 },
	{   0,   0, 255 },
	{   0,   0, 255 },
	{ 255,   0,   0 },
	{   0, 255, 255 },
	{ 255,   0,   0 },
	{ 255,   0,   0 },
	{ 255, 255,   0 },
	{ 255, 255,   0 },
	{   0, 255,   0 },
	{ 255,   0, 255 },
	{ 255, 255, 255 },
	{ 255, 255, 255 },
}};

// Source: TMS9918/28/29 Data Book, page 2-17.

static constexpr std::array<std::array<float, 3>, 16> TMS9XXXA_ANALOG_OUTPUT = {
	//           Y     R-Y    B-Y    voltages
	std::array{0.00f, 0.47f, 0.47f},
	std::array{0.00f, 0.47f, 0.47f},
	std::array{0.53f, 0.07f, 0.20f},
	std::array{0.67f, 0.17f, 0.27f},
	std::array{0.40f, 0.40f, 1.00f},
	std::array{0.53f, 0.43f, 0.93f},
	std::array{0.47f, 0.83f, 0.30f},
	std::array{0.73f, 0.00f, 0.70f},
	std::array{0.53f, 0.93f, 0.27f},
	std::array{0.67f, 0.93f, 0.27f},
	std::array{0.73f, 0.57f, 0.07f},
	std::array{0.80f, 0.57f, 0.17f},
	std::array{0.47f, 0.13f, 0.23f},
	std::array{0.53f, 0.73f, 0.67f},
	std::array{0.80f, 0.47f, 0.47f},
	std::array{1.00f, 0.47f, 0.47f},
};

std::array<std::array<uint8_t, 3>, 16> VDP::getMSX1Palette() const
{
	assert(isMSX1VDP());
	if (MSXDevice::getDeviceConfig().findChild("3bitrgboutput") != nullptr) {
		return THREE_BIT_RGB_PALETTE;
	}
	if ((version & VM_TOSHIBA_PALETTE) != 0) {
		return TOSHIBA_PALETTE;
	}
	if ((version & VM_YM2220_PALETTE) != 0) {
		return YM2220_PALETTE;
	}
	std::array<std::array<uint8_t, 3>, 16> tmsPalette;
	for (auto color : xrange(16)) {
		// convert from analog output to YPbPr
		float Y  = TMS9XXXA_ANALOG_OUTPUT[color][0];
		float Pr = TMS9XXXA_ANALOG_OUTPUT[color][1] - 0.5f;
		float Pb = TMS9XXXA_ANALOG_OUTPUT[color][2] - 0.5f;
		// apply the saturation
		Pr *= (narrow<float>(saturationPr) * (1.0f / 100.0f));
		Pb *= (narrow<float>(saturationPb) * (1.0f / 100.0f));
		// convert to RGB as follows:
		/*
		  |R|   | 1  0      1.402 |   |Y |
		  |G| = | 1 -0.344 -0.714 | x |Pb|
		  |B|   | 1  1.722  0     |   |Pr|
		*/
		float R = Y +           0 + 1.402f * Pr;
		float G = Y - 0.344f * Pb - 0.714f * Pr;
		float B = Y + 1.722f * Pb +           0;
		// blow up with factor of 255
		R *= 255;
		G *= 255;
		B *= 255;
		// the final result is that these values
		// are clipped in the [0:255] range.
		// Note: Using roundf instead of std::round because libstdc++ when
		//       built on top of uClibc lacks std::round; uClibc does provide
		//       roundf, but lacks other C99 math functions and that makes
		//       libstdc++ disable all wrappers for C99 math functions.
		tmsPalette[color][0] = Math::clipIntToByte(narrow_cast<int>(roundf(R)));
		tmsPalette[color][1] = Math::clipIntToByte(narrow_cast<int>(roundf(G)));
		tmsPalette[color][2] = Math::clipIntToByte(narrow_cast<int>(roundf(B)));
		// std::cerr << color << " " << int(tmsPalette[color][0]) << " " << int(tmsPalette[color][1]) <<" " << int(tmsPalette[color][2]) << '\n';
	}
	return tmsPalette;
}

// RegDebug

VDP::RegDebug::RegDebug(const VDP& vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(),
	                   vdp_.getName() + " regs", "VDP registers.", 0x40)
{
}

byte VDP::RegDebug::read(unsigned address)
{
	const auto& vdp = OUTER(VDP, vdpRegDebug);
	return vdp.peekRegister(address);
}

void VDP::RegDebug::write(unsigned address, byte value, EmuTime::param time)
{
	auto& vdp = OUTER(VDP, vdpRegDebug);
	// Ignore writes to registers >= 8 on MSX1. An alternative is to only
	// expose 8 registers. But changing that now breaks backwards
	// compatibility with some existing scripts. E.g. script that queries
	// PAL vs NTSC in a VDP agnostic way.
	if ((address >= 8) && vdp.isMSX1VDP()) return;
	vdp.changeRegister(narrow<byte>(address), value, time);
}


// StatusRegDebug

VDP::StatusRegDebug::StatusRegDebug(const VDP& vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(),
	                   vdp_.getName() + " status regs", "VDP status registers.", 0x10)
{
}

byte VDP::StatusRegDebug::read(unsigned address, EmuTime::param time)
{
	const auto& vdp = OUTER(VDP, vdpStatusRegDebug);
	return vdp.peekStatusReg(narrow<byte>(address), time);
}


// PaletteDebug

VDP::PaletteDebug::PaletteDebug(const VDP& vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(),
	                   vdp_.getName() + " palette", "V99x8 palette (RBG format)", 0x20)
{
}

byte VDP::PaletteDebug::read(unsigned address)
{
	const auto& vdp = OUTER(VDP, vdpPaletteDebug);
	word grb = vdp.getPalette(address / 2);
	return (address & 1) ? narrow_cast<byte>(grb >> 8)
	                     : narrow_cast<byte>(grb & 0xff);
}

void VDP::PaletteDebug::write(unsigned address, byte value, EmuTime::param time)
{
	auto& vdp = OUTER(VDP, vdpPaletteDebug);
	// Ignore writes on MSX1. An alternative could be to not expose the
	// palette at all, but allowing read-only access could be useful for
	// some scripts.
	if (vdp.isMSX1VDP()) return;

	unsigned index = address / 2;
	word grb = vdp.getPalette(index);
	grb = (address & 1)
	    ? word((grb & 0x0077) | ((value & 0x07) << 8))
	    : word((grb & 0x0700) | ((value & 0x77) << 0));
	vdp.setPalette(index, grb, time);
}


// class VRAMPointerDebug

VDP::VRAMPointerDebug::VRAMPointerDebug(const VDP& vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(), vdp_.getName() == "VDP" ?
			"VRAM pointer" : vdp_.getName() + " VRAM pointer",
			"VDP VRAM pointer (14 lower bits)", 2)
{
}

byte VDP::VRAMPointerDebug::read(unsigned address)
{
	const auto& vdp = OUTER(VDP, vramPointerDebug);
	if (address & 1) {
		return narrow_cast<byte>(vdp.vramPointer >> 8);  // TODO add read/write mode?
	} else {
		return narrow_cast<byte>(vdp.vramPointer & 0xFF);
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

// class RegisterLatchStatusDebug

VDP::RegisterLatchStatusDebug::RegisterLatchStatusDebug(const VDP &vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(),
			vdp_.getName() + " register latch status", "V99x8 register latch status (0 = expecting a value, 1 = expecting a register)", 1)
{
}

byte VDP::RegisterLatchStatusDebug::read(unsigned /*address*/)
{
	const auto& vdp = OUTER(VDP, registerLatchStatusDebug);
	return byte(vdp.registerDataStored);
}

// class VramAccessStatusDebug

VDP::VramAccessStatusDebug::VramAccessStatusDebug(const VDP &vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(), vdp_.getName() == "VDP" ?
			"VRAM access status" : vdp_.getName() + " VRAM access status",
			"VDP VRAM access status (0 = ready to read, 1 = ready to write)", 1)
{
}

byte VDP::VramAccessStatusDebug::read(unsigned /*address*/)
{
	const auto& vdp = OUTER(VDP, vramAccessStatusDebug);
	return byte(vdp.writeAccess);
}

// class PaletteLatchStatusDebug

VDP::PaletteLatchStatusDebug::PaletteLatchStatusDebug(const VDP &vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(),
			vdp_.getName() + " palette latch status", "V99x8 palette latch status (0 = expecting red & blue, 1 = expecting green)", 1)
{
}

byte VDP::PaletteLatchStatusDebug::read(unsigned /*address*/)
{
	const auto& vdp = OUTER(VDP, paletteLatchStatusDebug);
	return byte(vdp.paletteDataStored);
}

// class DataLatchDebug

VDP::DataLatchDebug::DataLatchDebug(const VDP &vdp_)
	: SimpleDebuggable(vdp_.getMotherBoard(),
			vdp_.getName() + " data latch value", "V99x8 data latch value (byte)", 1)
{
}

byte VDP::DataLatchDebug::read(unsigned /*address*/)
{
	const auto& vdp = OUTER(VDP, dataLatchDebug);
	return vdp.dataLatch;
}

// class Info

VDP::Info::Info(VDP& vdp_, const std::string& name_, std::string helpText_)
	: InfoTopic(vdp_.getMotherBoard().getMachineInfoCommand(),
		    strCat(vdp_.getName(), '_', name_))
	, vdp(vdp_)
	, helpText(std::move(helpText_))
{
}

void VDP::Info::execute(std::span<const TclObject> /*tokens*/, TclObject& result) const
{
	result = calc(vdp.getCurrentTime());
}

std::string VDP::Info::help(std::span<const TclObject> /*tokens*/) const
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
	return vdp.getMSXPos(time).y;
}


// class MsxX256PosInfo

VDP::MsxX256PosInfo::MsxX256PosInfo(VDP& vdp_)
	: Info(vdp_, "msx_x256_pos",
	       "Similar to 'cycle_in_frame', but expressed in MSX "
	       "coordinates. So a position in the left border has "
	       "a negative coordinate and a position in the right "
	       "border has a coordinate bigger or equal to 256. "
	       "See also 'msx_x512_pos'.")
{
}

int VDP::MsxX256PosInfo::calc(const EmuTime& time) const
{
	return vdp.getMSXPos(time).x / 2;
}


// class MsxX512PosInfo

VDP::MsxX512PosInfo::MsxX512PosInfo(VDP& vdp_)
	: Info(vdp_, "msx_x512_pos",
	       "Similar to 'cycle_in_frame', but expressed in "
	       "'narrow' (screen 7) MSX coordinates. So a position "
	       "in the left border has a negative coordinate and "
	       "a position in the right border has a coordinate "
	       "bigger or equal to 512. See also 'msx_x256_pos'.")
{
}

int VDP::MsxX512PosInfo::calc(const EmuTime& time) const
{
	return vdp.getMSXPos(time).x;
}


// version 1: initial version
// version 2: added frameCount
// version 3: removed verticalAdjust
// version 4: removed lineZero
// version 5: replace readAhead->cpuVramData, added cpuVramReqIsRead
// version 6: added cpuVramReqAddr to solve too_fast_vram_access issue
// version 7: removed cpuVramReqAddr again, fixed issue in a different way
// version 8: removed 'userData' from Schedulable
// version 9: update sprite-enabled-status only once per line
// version 10: added writeAccess
template<typename Archive>
void VDP::serialize(Archive& ar, unsigned serVersion)
{
	ar.template serializeBase<MSXDevice>(*this);

	if (ar.versionAtLeast(serVersion, 8)) {
		ar.serialize("syncVSync",         syncVSync,
		             "syncDisplayStart",  syncDisplayStart,
		             "syncVScan",         syncVScan,
		             "syncHScan",         syncHScan,
		             "syncHorAdjust",     syncHorAdjust,
		             "syncSetMode",       syncSetMode,
		             "syncSetBlank",      syncSetBlank,
		             "syncCpuVramAccess", syncCpuVramAccess);
		             // no need for syncCmdDone (only used for probe)
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

	ar.serialize("irqVertical",          irqVertical,
	             "irqHorizontal",        irqHorizontal,
	             "frameStartTime",       frameStartTime,
	             "displayStartSyncTime", displayStartSyncTime,
	             "vScanSyncTime",        vScanSyncTime,
	             "hScanSyncTime",        hScanSyncTime,
	             "displayStart",         displayStart,
	             "horizontalScanOffset", horizontalScanOffset,
	             "horizontalAdjust",     horizontalAdjust,
	             "registers",            controlRegs,
	             "blinkCount",           blinkCount,
	             "vramPointer",          vramPointer,
	             "palette",              palette,
	             "isDisplayArea",        isDisplayArea,
	             "palTiming",            palTiming,
	             "interlaced",           interlaced,
	             "statusReg0",           statusReg0,
	             "statusReg1",           statusReg1,
	             "statusReg2",           statusReg2,
	             "blinkState",           blinkState,
	             "dataLatch",            dataLatch,
	             "registerDataStored",   registerDataStored,
	             "paletteDataStored",    paletteDataStored);
	if (ar.versionAtLeast(serVersion, 5)) {
		ar.serialize("cpuVramData",      cpuVramData,
		             "cpuVramReqIsRead", cpuVramReqIsRead);
	} else {
		ar.serialize("readAhead", cpuVramData);
	}
	ar.serialize("cpuExtendedVram", cpuExtendedVram,
	             "displayEnabled",  displayEnabled);
	byte mode = displayMode.getByte();
	ar.serialize("displayMode", mode);
	displayMode.setByte(mode);

	ar.serialize("cmdEngine",     *cmdEngine,
	             "spriteChecker", *spriteChecker, // must come after displayMode
	             "vram",          *vram); // must come after controlRegs and after spriteChecker
	if constexpr (Archive::IS_LOADER) {
		pendingCpuAccess = syncCpuVramAccess.pendingSyncPoint();
		update(tooFastAccess);
	}

	if (ar.versionAtLeast(serVersion, 2)) {
		ar.serialize("frameCount", frameCount);
	} else {
		assert(Archive::IS_LOADER);
		// We could estimate the frameCount (assume framerate was
		// constant the whole time). But I think it's better to have
		// an obviously wrong value than an almost correct value.
		frameCount = 0;
	}

	if (ar.versionAtLeast(serVersion, 9)) {
		ar.serialize("syncSetSprites", syncSetSprites);
		ar.serialize("spriteEnabled", spriteEnabled);
	} else {
		assert(Archive::IS_LOADER);
		spriteEnabled = (controlRegs[8] & 0x02) == 0;
	}
	if (ar.versionAtLeast(serVersion, 10)) {
		ar.serialize("writeAccess", writeAccess);
	} else {
		writeAccess = !cpuVramReqIsRead; // best guess
	}

	// externalVideo does not need serializing. It is set on load by the
	// external video source (e.g. PioneerLDControl).
	//
	// TODO should superimposing be serialized? It cannot be recalculated
	// from register values (it depends on the register values at the start
	// of this frame). But it will be correct at the start of the next
	// frame. Probably good enough.

	if constexpr (Archive::IS_LOADER) {
		renderer->reInit();
	}
}
INSTANTIATE_SERIALIZE_METHODS(VDP);
REGISTER_MSXDEVICE(VDP, "VDP");

} // namespace openmsx
