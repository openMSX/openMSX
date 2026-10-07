#include "SMSVDP.hh"

#include "SMSVDPRenderer.hh"

#include "Display.hh"
#include "MSXMotherBoard.hh"
#include "PostProcessor.hh"
#include "Reactor.hh"
#include "RendererFactory.hh"
#include "VDP.hh"

#include "serialize.hh"

#include "narrow.hh"
#include "strCat.hh"

#include <algorithm>

namespace openmsx {

SMSVDP::SMSVDP(MSXMotherBoard& motherBoard_, std::string name)
	: motherBoard(motherBoard_)
	, videoStandardSetting(
		motherBoard.getCommandController(),
		"franky_video_standard",
		"Video standard of the Franky's Sega VDP "
		"(NTSC also covers the PAL-M timing)",
		VideoStandard::NTSC,
		EnumSetting<VideoStandard>::Map{
			{"NTSC", VideoStandard::NTSC},
			{"PAL",  VideoStandard::PAL}})
	, variantSetting(
		motherBoard.getCommandController(),
		"franky_vdp_variant",
		"Sega VDP chip variant of the Franky "
		"(SMS1 = 315-5124, SMS2 = 315-5246)",
		SMSVDPCore::Variant::SMS2,
		EnumSetting<SMSVDPCore::Variant>::Map{
			{"SMS1", SMSVDPCore::Variant::SMS1},
			{"SMS2", SMSVDPCore::Variant::SMS2}})
	, autoVideoSwitchSetting(
		motherBoard.getCommandController(),
		"franky_auto_video_switch",
		"Automatically select the video source: the MSX VDP while it has "
		"interrupts enabled, otherwise Franky while its VDP has interrupts "
		"enabled. Disable to control the 'videosource' setting manually.",
		true)
	, core(variantSetting.getEnum(), isPal())
	, irq(motherBoard, name + ".IRQ")
	, regDebug(*this, name)
	, statusDebug(*this, name)
	, pixelClock(EmuTime::zero())
{
	updateClock();
	videoStandardSetting.attach(*this);
	variantSetting.attach(*this);
	motherBoard.getReactor().getDisplay().attach(*this);
	auto time = motherBoard.getCurrentTime();
	createRenderer(time);
	reset(time);
}

SMSVDP::~SMSVDP()
{
	motherBoard.getReactor().getDisplay().detach(*this);
	variantSetting.detach(*this);
	videoStandardSetting.detach(*this);
}

void SMSVDP::reset(EmuTime time)
{
	syncLine.removeSyncPoint();
	syncInt.removeSyncPoint();
	syncDraw.removeSyncPoint();
	syncEol.removeSyncPoint();
	core.setPal(isPal()); // the setting is authoritative
	core.reset();
	vpos = core.height() - 1; // executeLine() will wrap to 0
	frameActive = false;
	pixelClock.reset(time);
	renderer->reset(time);
	syncLine.setSyncPoint(time);
	updateIrq();
}

// ---------------------------------------------------------------------------
// ports 0x88/0x89, 0x48/0x49
// ---------------------------------------------------------------------------

uint8_t SMSVDP::readData(EmuTime /*time*/)
{
	auto result = core.readData();
	updateIrq();
	return result;
}

void SMSVDP::writeData(uint8_t value, EmuTime /*time*/)
{
	core.writeData(value);
}

uint8_t SMSVDP::readControl(EmuTime time)
{
	auto result = core.readControl(narrow<int>(currentHpos(time)));
	updateIrq();
	return result;
}

void SMSVDP::writeControl(uint8_t value, EmuTime time)
{
	core.writeControl(value, narrow<int>(currentHpos(time)));
	updateIrq();
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
		renderer->frameStart(time);
		frameActive = true;
		updateAutoVideoSwitch(time);
	}
	pixelClock.reset(time);
	core.processLine(vpos);

	// The chip has a single /INT pin, shared by the VINT and HINT logic,
	// so one event at the latest of the two hpos moments covers both.
	syncInt .setSyncPoint(pixelClock + core.intHpos());
	syncDraw.setSyncPoint(pixelClock + 63); // MAME's DRAW_TIME_SMS
	syncEol .setSyncPoint(pixelClock + (SMSVDPCore::WIDTH - 1));
	syncLine.setSyncPoint(pixelClock + SMSVDPCore::WIDTH);
	updateIrq();
}

void SMSVDP::executeInt(EmuTime /*time*/)
{
	core.updateInterrupts(core.intHpos());
	updateIrq();
}

void SMSVDP::executeDraw(EmuTime /*time*/)
{
	// No need to render scanlines when this video source is not visible.
	if (!renderer->isActive()) return;

	const int outLine = vpos - core.activeStart() +
	                    (SMSVDPCore::OUTPUT_HEIGHT - core.yPixels()) / 2;
	if (0 <= outLine && outLine < SMSVDPCore::OUTPUT_HEIGHT) {
		std::array<uint32_t, SMSVDPCore::OUTPUT_WIDTH> pixels;
		core.drawLine(outLine, pixels);
		renderer->drawLine(narrow<unsigned>(outLine), pixels);
	}
}

void SMSVDP::executeEol(EmuTime /*time*/)
{
	core.checkPendingFlags(SMSVDPCore::WIDTH - 1);
	updateIrq();
}

void SMSVDP::updateClock()
{
	// The Franky uses the MSX bus clock for NTSC and its dedicated PAL
	// crystal for PAL (3.579545 MHz and 3.546893 MHz respectively); the
	// VDP output pixel clock is half of the VDP master clock (which is 3x
	// the input clock).
	pixelClock.setFreq(isPal() ? 10640679u : 10738635u, 2u);
}

void SMSVDP::reschedule(EmuTime time)
{
	// Called when a setting changes mid-emulation: cut off the current
	// line and continue with the (possibly different) timing from 'time'.
	syncLine.removeSyncPoint();
	syncInt.removeSyncPoint();
	syncDraw.removeSyncPoint();
	syncEol.removeSyncPoint();
	pixelClock.reset(time);
	syncLine.setSyncPoint(time);
}

unsigned SMSVDP::currentHpos(EmuTime time) const
{
	// The scheduler guarantees that all sync points before 'time' have
	// been executed, so pixelClock's last tick is not after 'time'.
	return std::min<unsigned>(SMSVDPCore::WIDTH - 1, pixelClock.getTicksTill(time));
}

void SMSVDP::updateIrq()
{
	irq.set(core.intLineAsserted());
}

// ---------------------------------------------------------------------------
// renderer / video source
// ---------------------------------------------------------------------------

void SMSVDP::createRenderer(EmuTime time)
{
	renderer = RendererFactory::createSMSVDPRenderer(
		*this, motherBoard.getReactor().getDisplay());
	renderer->reset(time);
}

PostProcessor* SMSVDP::getPostProcessor() const
{
	return renderer->getPostProcessor();
}

Scheduler& SMSVDP::getScheduler() const
{
	return motherBoard.getScheduler();
}

void SMSVDP::updateAutoVideoSwitch(EmuTime time)
{
	if (!autoVideoSwitchSetting.getBoolean()) return;

	// The machine's VDP always uses the id "VDP" in the machine configs.
	auto* vdp = dynamic_cast<VDP*>(motherBoard.findDevice("VDP"));
	if (!vdp) return;

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
	createRenderer(motherBoard.getCurrentTime());
}

// ---------------------------------------------------------------------------
// settings
// ---------------------------------------------------------------------------

void SMSVDP::update(const Setting& setting) noexcept
{
	if (&setting == &videoStandardSetting) {
		// The core's frame geometry (262 vs 313 lines, v-counter wrap,
		// active area) must follow the video standard too, not only the
		// pixel clock.
		core.setPal(isPal());
		vpos = std::min(vpos, core.height() - 1);
		updateClock();
		reschedule(motherBoard.getCurrentTime());
		updateIrq();
	} else if (&setting == &variantSetting) {
		core = SMSVDPCore(variantSetting.getEnum(), isPal());
		vpos = std::min(vpos, core.height() - 1);
		reschedule(motherBoard.getCurrentTime());
		updateIrq();
	}
}

// ---------------------------------------------------------------------------
// debuggables
// ---------------------------------------------------------------------------

SMSVDP::RegDebug::RegDebug(const SMSVDP& vdp, std::string_view name)
	: SimpleDebuggable(vdp.getMotherBoard(),
	                   strCat(name, " regs"), "Franky VDP registers.", 16)
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
	vdp.core.writeRegister(narrow<uint8_t>(address), value,
	                       narrow<int>(vdp.currentHpos(time)));
	vdp.updateIrq();
}

SMSVDP::StatusDebug::StatusDebug(const SMSVDP& vdp, std::string_view name)
	: SimpleDebuggable(vdp.getMotherBoard(),
	                   strCat(name, " status regs"), "Franky VDP status register.", 1)
{
}

uint8_t SMSVDP::StatusDebug::read(unsigned /*address*/)
{
	const auto& vdp = OUTER(SMSVDP, statusDebug);
	return vdp.core.peekStatus();
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
	if constexpr (Archive::IS_LOADER) {
		core.setPal(isPal()); // the setting is authoritative
		vpos = std::min(vpos, core.height() - 1);
		updateClock();
		updateIrq();
	}
}
INSTANTIATE_SERIALIZE_METHODS(SMSVDP);

} // namespace openmsx
