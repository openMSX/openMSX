#include "SMSVDP.hh"

#include "SMSVDPDummyRenderer.hh"
#include "SMSVDPRenderer.hh"

#include "Debugger.hh"
#include "Display.hh"
#include "MSXCliComm.hh"
#include "MSXMotherBoard.hh"
#include "PostProcessor.hh"
#include "Reactor.hh"
#include "RendererFactory.hh"
#include "VDP.hh"

#include "serialize.hh"

#include "narrow.hh"
#include "strCat.hh"

#include <algorithm>
#include <cassert>

namespace openmsx {

namespace {

// The debuggables are chip-scoped ("SMS VDP ..."), not device-scoped: they
// belong to the VDP. Make the base name unique in case a second VDP instance
// is created (a second Franky cannot output video, but its device is still
// constructed). Same " (n)" scheme as MSXDevice and SoundDevice.
[[nodiscard]] std::string makeUniqueDebugBase(MSXMotherBoard& motherBoard, std::string_view base)
{
	auto& debugger = motherBoard.getDebugger();
	auto isFree = [&](std::string_view b) {
		return !debugger.findDebuggable(strCat(b, " regs")) &&
		       !debugger.findDebuggable(strCat(b, " status regs")) &&
		       !debugger.findDebuggable(strCat(b, " VRAM")) &&
		       !debugger.findDebuggable(strCat(b, " CRAM"));
	};
	std::string result(base);
	for (unsigned n = 1; !isFree(result); ++n) {
		result = strCat(base, " (", n, ')');
	}
	return result;
}

} // namespace

SMSVDP::SMSVDP(MSXMotherBoard& motherBoard_, std::string name,
               VideoStandard standard, SMSVDPCore::Variant variant)
	: motherBoard(motherBoard_)
	, videoStandard(standard)
	, autoVideoSwitchSetting(
		motherBoard.getCommandController(),
		name + " auto video switch",
		"Automatically select the video source: the MSX VDP while it has "
		"interrupts enabled, otherwise Franky while its VDP has interrupts "
		"enabled. Disable to control the 'videosource' setting manually.",
		true)
	, irq(motherBoard, name + ".IRQ")
	, core(variant, isPal(), [this](bool asserted) { irq.set(asserted); })
	, debugName(makeUniqueDebugBase(motherBoard_, "SMS VDP"))
	, regDebug(*this, debugName)
	, statusDebug(*this, debugName)
	, vramDebug(*this, debugName)
	, cramDebug(*this, debugName)
	, pixelClock(EmuTime::zero())
{
	updateClock();
	motherBoard.getReactor().getDisplay().attach(*this);
	auto time = motherBoard.getCurrentTime();
	createRenderer();
	reset(time);
}

SMSVDP::~SMSVDP()
{
	motherBoard.getReactor().getDisplay().detach(*this);
}

void SMSVDP::reset(EmuTime time)
{
	syncLine.removeSyncPoint();
	syncInt.removeSyncPoint();
	syncDraw.removeSyncPoint();
	syncEol.removeSyncPoint();
	core.setPal(isPal()); // the device configuration is authoritative
	core.reset();
	renderer->reInit();
	vpos = core.height() - 1; // executeLine() will wrap to 0
	frameActive = false;
	pixelClock.reset(time);
	syncLine.setSyncPoint(time);
}

// ---------------------------------------------------------------------------
// ports 0x88/0x89, 0x48/0x49
// ---------------------------------------------------------------------------

uint8_t SMSVDP::readData(EmuTime /*time*/)
{
	return core.readData();
}

void SMSVDP::writeData(uint8_t value, EmuTime /*time*/)
{
	core.writeData(value);
}

uint8_t SMSVDP::readControl(EmuTime time)
{
	return core.readControl(narrow<int>(currentHpos(time)));
}

void SMSVDP::writeControl(uint8_t value, EmuTime time)
{
	core.writeControl(value, narrow<int>(currentHpos(time)));
}

uint8_t SMSVDP::readVCounter(EmuTime time)
{
	return core.readVCounter(vpos, narrow<int>(currentHpos(time)));
}

uint8_t SMSVDP::readHCounter(EmuTime /*time*/)
{
	// The H counter is only valid after it has been latched (the latch is
	// cleared at the start of the last top-border line). Like MAME, the
	// latch is only set through latchHCounter().
	return core.readHCounter();
}

uint8_t SMSVDP::peekVCounter(EmuTime time) const
{
	return core.readVCounter(vpos, narrow<int>(currentHpos(time)));
}

uint8_t SMSVDP::peekHCounter() const
{
	return core.peekHCounter();
}

uint8_t SMSVDP::peekData() const
{
	return core.peekData();
}

uint8_t SMSVDP::peekControl(EmuTime time) const
{
	return core.peekControl(narrow<int>(currentHpos(time)));
}

// ---------------------------------------------------------------------------
// scheduling
// ---------------------------------------------------------------------------

void SMSVDP::executeLine(EmuTime time)
{
	if (++vpos >= core.height()) {
		vpos = 0;
	}
	if (vpos == 0) {
		if (frameActive) {
			renderer->frameEnd(time);
		}
		frameActive = true;
		renderer->frameStart(time);
		updateAutoVideoSwitch(time);
	}
	pixelClock.reset(time);
	core.processLine(vpos);

	// The chip has a single /INT pin, shared by the VINT and HINT logic,
	// so one event at the latest of the two hpos moments covers both.
	syncInt .setSyncPoint(pixelClock + core.intHpos());
	syncDraw.setSyncPoint(pixelClock + SMSVDPCore::ACTIVE_HPOS); // MAME's DRAW_TIME_SMS
	syncEol .setSyncPoint(pixelClock + (SMSVDPCore::WIDTH - 1));
	syncLine.setSyncPoint(pixelClock + SMSVDPCore::WIDTH);
}

void SMSVDP::executeInt(EmuTime /*time*/)
{
	core.updateInterrupts(core.intHpos());
}

void SMSVDP::executeDraw(EmuTime /*time*/)
{
	// No need to render scanlines when this video source is not visible.
	if (!renderer->isActive()) return;

	const int outLine = vpos - core.activeStart() +
	                    (SMSVDPCore::OUTPUT_HEIGHT - core.yPixels()) / 2;
	if (0 <= outLine && outLine < SMSVDPCore::OUTPUT_HEIGHT) {
		std::array<uint8_t, SMSVDPCore::OUTPUT_WIDTH> paletteIndices;
		core.drawLine(outLine, paletteIndices);
		renderer->drawLine(narrow<unsigned>(outLine), paletteIndices);
	}
}

void SMSVDP::executeEol(EmuTime /*time*/)
{
	core.checkPendingFlags(SMSVDPCore::WIDTH - 1);
}

void SMSVDP::updateClock()
{
	// The Franky uses the MSX bus clock for NTSC and its dedicated PAL
	// crystal for PAL (3.579545 MHz and 3.546893 MHz respectively); the
	// VDP output pixel clock is half of the VDP master clock (which is 3x
	// the input clock).
	pixelClock.setFreq(isPal() ? 10640679u : 10738635u, 2u);
}

unsigned SMSVDP::currentHpos(EmuTime time) const
{
	// The scheduler guarantees that all sync points before 'time' have
	// been executed, so pixelClock's last tick is not after 'time'.
	return std::min<unsigned>(SMSVDPCore::WIDTH - 1, pixelClock.getTicksTill(time));
}

std::pair<int, int> SMSVDP::getRasterPosition() const
{
	// Active-display coordinates: (0,0) is the top-left of the 256x192
	// (or 224/240) display area; values outside that area are possible.
	const auto time = motherBoard.getCurrentTime();
	return { int(currentHpos(time)) - SMSVDPCore::ACTIVE_HPOS,
	         vpos - core.activeStart() };
}

void SMSVDP::changeRegister(uint8_t reg, uint8_t val, EmuTime time)
{
	core.writeRegister(reg, val, narrow<int>(currentHpos(time)));
}

// ---------------------------------------------------------------------------
// renderer / video source
// ---------------------------------------------------------------------------

void SMSVDP::createRenderer()
{
	// Only one Franky can provide the "Franky" video source. A second
	// Franky is still emulated (both devices respond to the shared I/O
	// ports), but its video output cannot be selected. Two Frankys can
	// never work together anyway: they generate interrupts at independent
	// times, so software could not tell which VDP interrupted.
	const auto sources = motherBoard.getVideoSource().getPossibleValues();
	if (std::ranges::find(sources, VIDEO_SOURCE) != sources.end()) {
		motherBoard.getMSXCliComm().printWarning(
			"Another Franky is already providing the \"", VIDEO_SOURCE,
			"\" video source; the video output of this Franky is disabled.");
		renderer = std::make_unique<SMSVDPDummyRenderer>();
	} else {
		renderer = RendererFactory::createSMSVDPRenderer(
			*this, motherBoard.getReactor().getDisplay());
	}
}

PostProcessor* SMSVDP::getPostProcessor() const
{
	return renderer->getPostProcessor();
}

Scheduler& SMSVDP::getScheduler() const
{
	return motherBoard.getScheduler();
}

void SMSVDP::setMsxVdp(VDP& vdp)
{
	msxVdp = &vdp;
}

void SMSVDP::updateAutoVideoSwitch(EmuTime time)
{
	if (!autoVideoSwitchSetting.getBoolean()) return;
	// A Franky without video output (dummy renderer) must not manage the
	// video source.
	if (getPostProcessor() == nullptr) return;

	// The VDP reference is resolved once in Franky::init(); openMSX
	// guarantees it stays valid as long as this Franky exists.
	auto* vdp = msxVdp;
	assert(vdp);

	// Interrupts enabled: IE0 = R#1 bit 5 (all VDPs), IE1 = R#0 bit 4
	// (V99x8 only; R#1 bit 4 is M1, a display-mode bit!).
	bool msxEnabled = (vdp->peekRegister(1, time) & 0x20) != 0;
	if (!vdp->isMSX1VDP()) {
		msxEnabled |= (vdp->peekRegister(0, time) & 0x10) != 0;
	}
	int desired = 0;
	if (msxEnabled) {
		// The MSX VDP generates interrupts: always prefer it.
		if (auto* postProcessor = vdp->getPostProcessor()) {
			desired = postProcessor->getVideoSource();
		}
	} else if (core.interruptsEnabled()) {
		// Only Franky generates interrupts: show its output.
		if (auto* postProcessor = getPostProcessor()) {
			desired = postProcessor->getVideoSource();
		}
	}

	auto& videoSource = motherBoard.getVideoSource();
	if (desired && (videoSource.getSource() != desired)) {
		videoSource.setSource(desired);
	}
}

void SMSVDP::preVideoSystemChange() noexcept
{
	renderer.reset();
}

void SMSVDP::postVideoSystemChange() noexcept
{
	createRenderer();
}

// ---------------------------------------------------------------------------
// debuggables
// ---------------------------------------------------------------------------

SMSVDP::RegDebug::RegDebug(const SMSVDP& vdp, std::string_view name_)
	: SimpleDebuggable(vdp.getMotherBoard(),
	                   strCat(name_, " regs"), "SMS VDP registers.", 16)
{
}

uint8_t SMSVDP::RegDebug::read(unsigned address)
{
	const auto& vdp = OUTER(SMSVDP, regDebug);
	return vdp.core.peekReg(narrow<int>(address));
}

void SMSVDP::RegDebug::write(unsigned address, uint8_t value, EmuTime time)
{
	auto& vdp = OUTER(SMSVDP, regDebug);
	vdp.changeRegister(narrow<uint8_t>(address), value, time);
}

SMSVDP::StatusDebug::StatusDebug(const SMSVDP& vdp, std::string_view name_)
	: SimpleDebuggable(vdp.getMotherBoard(),
	                   strCat(name_, " status regs"), "SMS VDP status register.", 1)
{
}

uint8_t SMSVDP::StatusDebug::read(unsigned /*address*/, EmuTime time)
{
	const auto& vdp = OUTER(SMSVDP, statusDebug);
	return vdp.peekControl(time);
}

SMSVDP::VramDebug::VramDebug(const SMSVDP& vdp, std::string_view name_)
	: SimpleDebuggable(vdp.getMotherBoard(),
	                   strCat(name_, " VRAM"), "SMS VDP video RAM (16 kB).", 0x4000)
{
}

uint8_t SMSVDP::VramDebug::read(unsigned address)
{
	const auto& vdp = OUTER(SMSVDP, vramDebug);
	return vdp.core.peekVram(narrow<uint16_t>(address));
}

void SMSVDP::VramDebug::write(unsigned address, uint8_t value, EmuTime /*time*/)
{
	auto& vdp = OUTER(SMSVDP, vramDebug);
	vdp.core.pokeVram(narrow<uint16_t>(address), value);
}

SMSVDP::CramDebug::CramDebug(const SMSVDP& vdp, std::string_view name_)
	: SimpleDebuggable(vdp.getMotherBoard(),
	                   strCat(name_, " CRAM"), "SMS VDP color RAM (32 bytes, 6-bit colors).", 0x20)
{
}

uint8_t SMSVDP::CramDebug::read(unsigned address)
{
	const auto& vdp = OUTER(SMSVDP, cramDebug);
	return vdp.core.peekCram(narrow<int>(address));
}

void SMSVDP::CramDebug::write(unsigned address, uint8_t value, EmuTime /*time*/)
{
	auto& vdp = OUTER(SMSVDP, cramDebug);
	vdp.core.pokeCram(narrow<int>(address), value);
}

// ---------------------------------------------------------------------------
// serialize
// ---------------------------------------------------------------------------

template<typename Archive>
void SMSVDP::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("core", core);
	ar.serialize("vpos", vpos);
	ar.serialize("frameActive", frameActive);
	ar.serialize("pixelClock", pixelClock);
	ar.serialize("syncLine", syncLine,
	             "syncInt",  syncInt,
	             "syncDraw", syncDraw,
	             "syncEol",  syncEol);
	ar.serialize("irq", irq);
	if constexpr (Archive::IS_LOADER) {
		core.setPal(isPal()); // the device configuration is authoritative
		vpos = std::min(vpos, core.height() - 1);
		renderer->reInit(); // a frame may be in progress, discard it
		updateClock();
	}
}
INSTANTIATE_SERIALIZE_METHODS(SMSVDP);

} // namespace openmsx
