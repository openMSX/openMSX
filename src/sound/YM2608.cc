#include "YM2608.hh"

#include "DummyAY8910Periphery.hh"

#include "DeviceConfig.hh"
#include "Clock.hh"
#include "serialize.hh"

#include "3rdparty/ym2608/fmopn_2608rom.h"

namespace openmsx {

static constexpr unsigned CLOCK = 8'000'000;

static constexpr uint8_t STATUS_ADPCM_B_EOS = 0x04;
static constexpr uint8_t STATUS_ADPCM_B_BRDY = 0x08;
static constexpr uint8_t STATUS_ADPCM_B_PLAYING = 0x20;

YM2608::YM2608(DeviceConfig& config, std::string_view name, EmuTime time)
	: irq(config.getMotherBoard(), strCat(name, ".IRQ"))
	, timers{Timer(config.getScheduler(), *this, 0),
	         Timer(config.getScheduler(), *this, 1)}
	, contextTime(time)
	, busyEnd(time)
	, fm(*this)
	, adpcmA(*this)
	, adpcmB(*this)
	, sampleRAM(config, strCat(name, " ADPCM RAM"), "YM2608 ADPCM-B sample RAM", 0x40000)
	, registers(config.getMotherBoard(), name, *this)
	, fmPart(config, name, *this)
	, ssg(strCat(name, " SSG"), DummyAY8910Periphery::instance(), config, time,
		AY8910::Type::YM2149, 2'000'000.0f)
{
	// Maximum normalization. Standard SSG volume replaces the old trim.
	ssg.setSoftwareVolume(16382.0f * std::numbers::sqrt2_v<float> * (2.0f / 3.0f) / (32768.0f * 4.3f), time);

	sampleRAM.clear(0); // Deterministic emulator policy; hardware power-on contents are unknown.
	updatePrescale(fm.clock_prescale());
	reset(time);
}

void YM2608::reset(EmuTime time)
{
	updateStream(time);
	for (auto& timer : timers) {
		timer.cancel();
	}

	// reset the engines
	fm.reset();
	adpcmA.reset();
	adpcmB.reset();

	// configure ADPCM percussion sounds; these are present in an embedded ROM
	adpcmA.set_start_end(0, 0x0000, 0x01bf); // bass drum
	adpcmA.set_start_end(1, 0x01c0, 0x043f); // snare drum
	adpcmA.set_start_end(2, 0x0440, 0x1b7f); // top cymbal
	adpcmA.set_start_end(3, 0x1b80, 0x1cff); // high hat
	adpcmA.set_start_end(4, 0x1d00, 0x1f7f); // tom tom
	adpcmA.set_start_end(5, 0x1f80, 0x1fff); // rim shot

	// initialize our special interrupt states, then read the upper status
	// register, which updates the IRQs
	irqEnable = 0x1f;
	flagControl = 0x1c;
	readStatusHi();

	ssg.reset(time);
	applyRates();
	busyEnd = time;
	irq.reset();
}

uint8_t YM2608::readPort(unsigned port, EmuTime time)
{
	updateStream(time);

	switch (port & 3) {
	case 0: // status port, YM2203 compatible
		return readStatus();
	case 1: // data port (only SSG)
		return readData(time);
	case 2: // status port, extended
		return readStatusHi();
	case 3: // ADPCM-B data
		return readDataHi();
	}
	UNREACHABLE;
}

uint8_t YM2608::peekPort(unsigned port, EmuTime time) const
{
	// Debugger reads deliberately bypass readStatusHi()'s IRQ update and the
	// ADPCM data port's dummy reads, address advancement and flag changes.
	auto busy = (time < busyEnd) ? fm_engine::STATUS_BUSY : 0;
	switch (port & 3) {
	case 0:
		return (fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB)) | busy;
	case 1:
		if (addressLatch < 0x10) {
			return ssg.peekRegister(addressLatch, time);
		}
		return (addressLatch == 0xff) ? 1 : 0;
	case 2:
		return statusHi() | busy;
	case 3:
		return ((addressLatch & 0xff) < 0x10) ? adpcmB.peek(addressLatch & 0x0f) : 0;
	}
	UNREACHABLE;
}


uint8_t YM2608::readStatus()
{
	uint8_t result = fm.status() & (fm_engine::STATUS_TIMERA | fm_engine::STATUS_TIMERB);
	if (ymfm_is_busy()) {
		result |= fm_engine::STATUS_BUSY;
	}
	return result;
}

uint8_t YM2608::readData(EmuTime time)
{
	if (addressLatch < 0x10) {
		return ssg.readRegister(addressLatch & 0x0f, time);
	} else {
		return addressLatch == 0xff ? 1 : 0;
	}
}

uint8_t YM2608::readStatusHi()
{
	uint8_t status = statusHi();

	// update the status so that IRQs are propagated
	fm.set_reset_status(status, ~status);

	// merge in the busy flag
	if (ymfm_is_busy()) {
		status |= fm_engine::STATUS_BUSY;
	}
	return status;
}

uint8_t YM2608::statusHi() const
{
	// fetch regular status
	uint8_t status = fm.status() & ~(STATUS_ADPCM_B_EOS | STATUS_ADPCM_B_BRDY | STATUS_ADPCM_B_PLAYING);

	// fetch ADPCM-B status, and merge in the bits
	uint8_t adpcmStatus = adpcmB.status();
	if ((adpcmStatus & ymfm::adpcm_b_channel::STATUS_EOS) != 0) {
		status |= STATUS_ADPCM_B_EOS;
	}
	if ((adpcmStatus & ymfm::adpcm_b_channel::STATUS_BRDY) != 0) {
		status |= STATUS_ADPCM_B_BRDY;
	}
	if ((adpcmStatus & ymfm::adpcm_b_channel::STATUS_PLAYING) != 0) {
		status |= STATUS_ADPCM_B_PLAYING;
	}

	// turn off any bits that have been requested to be masked
	status &= ~(flagControl & 0x1f);

	return status;
}

uint8_t YM2608::readDataHi()
{
	if ((addressLatch & 0xff) < 0x10) {
		return adpcmB.read(addressLatch & 0x0f);
	} else {
		return 0;
	}
}

void YM2608::writePort(unsigned port, uint8_t value, EmuTime time)
{
	switch (port & 3) {
	case 0: // lower address port
		addressLatch = value;
		// special case: update the prescale
		if (0x2d <= addressLatch && addressLatch <= 0x2f) {
			if (addressLatch == 0x2d) {
				updatePrescale(6);
			} else if (addressLatch == 0x2e && fm.clock_prescale() == 6) {
				updatePrescale(3);
			} else if (addressLatch == 0x2f) {
				updatePrescale(2);
			}
		}
		break;
	case 2: // upper address port
		addressLatch = 0x100 | value;
		break;
	case 1: // lower data port
		if (addressLatch & 0x100) break; // ignore if paired with upper address
		writeRegister(addressLatch, value, time);
		break;
	case 3: // upper data port
		if ((addressLatch & 0x100) == 0) break; // ignore if paired with lower address
		writeRegister(addressLatch, value, time);
		break;
	}

	applyRates();
}

void YM2608::writeRegister(unsigned regnum, uint8_t data, EmuTime time)
{
	updateStream(time);

	if (regnum < 0x10) {
		// 00-0F: write to SSG
		ssg.writeRegister(addressLatch & 0x0f, data, time);
	} else if (regnum < 0x20) {
		// 10-1F: write to ADPCM-A
		adpcmA.write(regnum & 0x0f, data);
	} else if (regnum == 0x29) {
		// 29: special IRQ mask register
		irqEnable = data;
		fm.set_irq_mask(irqEnable & ~flagControl & 0x1f);
	} else if (0x100 <= regnum && regnum < 0x110) {
		// 100-10F: write to ADPCM-B
		adpcmB.write(regnum & 0x0f, data);
	} else if (regnum == 0x110) {
		// 110: IRQ flag control
		if (data & 0x80) {
			fm.set_reset_status(0, 0xff);
		} else {
			flagControl = data;
			fm.set_irq_mask(irqEnable & ~flagControl & 0x1f);
		}
	} else {
		// 20-28, 2A-FF, 111-1FF: write to FM
		fm.write(regnum, data);
	}

	// mark busy for a bit
	ymfm_set_busy_end(32 * fm.clock_prescale());
}

uint8_t YM2608::peekRegister(unsigned regnum, EmuTime time) const
{
	assert(regnum < 0x200);
	if (regnum < 0x10) {
		return ssg.peekRegister(regnum, time);
	} else if (regnum < 0x20) {
		return adpcmA.regs().read(regnum & 0x0f);
	} else if (regnum == 0x29) {
		return irqEnable;
	} else if (0x100 <= regnum && regnum < 0x110) {
		return adpcmB.regs().read(regnum & 0x0f);
	} else if (regnum == 0x110) {
		return flagControl;
	} else {
		return fm.regs().read(regnum);
	}
}

void YM2608::updateStream(EmuTime time)
{
	fmPart.updateStream(time);
	contextTime = time;
}

void YM2608::updatePrescale(uint8_t prescale)
{
	fm.set_clock_prescale(prescale);
	//ssg.prescale_changed();
}

unsigned YM2608::prescale() const
{
	return fm.clock_prescale();
}

unsigned YM2608::fmRate() const
{
	return (CLOCK + 12 * prescale()) / (24 * prescale());
}

unsigned YM2608::ssgRate() const
{
	return CLOCK / (prescale() == 6 ? 32
	              : prescale() == 3 ? 16
	                                :  8);
}

void YM2608::applyRates()
{
	fmPart.rate(fmRate());
	//ssg.rate(ssgRate());
}

void YM2608::ymfm_set_timer(uint32_t timer, int32_t duration)
{
	if (duration < 0) {
		timers[timer].cancel();
	} else {
		timers[timer].schedule(contextTime + Clock<CLOCK>::duration(unsigned(duration)));
	}
}

void YM2608::ymfm_set_busy_end(uint32_t duration)
{
	busyEnd = contextTime + Clock<CLOCK>::duration(duration);
}

bool YM2608::ymfm_is_busy()
{
	return contextTime < busyEnd;
}

void YM2608::ymfm_update_irq(bool asserted)
{
	irq.set(asserted);
}

uint8_t YM2608::ymfm_external_peek(ymfm::access_class type, uint32_t address)
{
	// Both sample stores are passive memory; GPIO is unconnected.
	return ymfm_external_read(type, address);
}

uint8_t YM2608::ymfm_external_read(ymfm::access_class type, uint32_t address)
{
	if (type == ymfm::ACCESS_ADPCM_B) {
		return sampleRAM[address & 0x3ffff];
	}
	if (type == ymfm::ACCESS_ADPCM_A) {
		return YM2608_ADPCM_ROM[address & 0x1fff];
	}
	return 0xff; // SSG GPIO is not attached to the MSX keyboard or joysticks.
}

void YM2608::ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t value)
{
	if (type == ymfm::ACCESS_ADPCM_B) {
		sampleRAM[address & 0x3ffff] = value;
	}
}

void YM2608::generateFM(std::span<float*> buffers, unsigned num)
{
	if (adpcmB.silent()) {
		buffers[6] = nullptr;
	}
	for (unsigned c = 0; c < 6; ++c) {
		if (adpcmA.silent(c)) {
			buffers[c + 7] = nullptr;
		}
	}
	// ADPCM-A replays this counter. generate() advances the engine's copy.
	const uint32_t env = fm.envelope_counter();
	// Bit 7 of register 0x29 enables FM channels 3-5. generate() nulls every
	// channel that is outside this mask or that prepare() finds already quiet.
	const uint32_t fmMask = (irqEnable & 0x80) ? 0x3f : 0x07;
	fm.generate(buffers.template first<6>(), num, fmMask);
	adpcmB.generate(buffers[6], num);
	adpcmA.generate(buffers.subspan(7, 6).template first<6>(), num, env);
}

template<typename Archive>
void YM2608::serialize(Archive& ar, unsigned /*version*/)
{
	if constexpr (!Archive::IS_LOADER) {
		updateStream(timers[0].getCurrentTime());
	}
	ar.serialize("timers",      timers,
	             "address",     addressLatch,
	             "irqEnable",   irqEnable,
	             "flagControl", flagControl,
	             "fm",          fm,
	             "adpcmA",      adpcmA,
	             "adpcmB",      adpcmB);
	if constexpr (Archive::IS_LOADER) {
		updatePrescale(fm.clock_prescale());
	}
	ar.serialize("busyEnd", busyEnd,
	             "sampleRAM", sampleRAM,
	             "irq", irq,
	             "sampleClock", fmPart.getEmuClock(),
	             "ssg", ssg);
	if constexpr (Archive::IS_LOADER) {
		const auto fmTime = fmPart.getEmuClock().getTime();
		//const auto ssgTime = ssg.getEmuClock().getTime();
		applyRates();
		fmPart.restoreClock(fmTime);
		//ssg.restoreClock(ssgTime);
		contextTime = timers[0].getCurrentTime();
	}
}
INSTANTIATE_SERIALIZE_METHODS(YM2608);


YM2608::FmPart::FmPart(DeviceConfig& config, std::string_view name_, YM2608& chip_)
	: ResampledSoundDevice(
		config.getMotherBoard(), name_,
		"Makoto FM, rhythm and ADPCM", 13, (CLOCK + 72) / 144, true)
	, chip(chip_)
{
	registerSound(config);
}

YM2608::FmPart::~FmPart()
{
	unregisterSound();
}

void YM2608::FmPart::updateStream(EmuTime time)
{
	SoundDevice::updateStream(time);
}

void YM2608::FmPart::restoreClock(EmuTime time)
{
	createResampler();
	getEmuClock().reset(time);
}

void YM2608::FmPart::rate(unsigned value)
{
	if (getInputRate() != value) {
		setInputRate(value);
		createResampler();
	}
}

void YM2608::FmPart::setOutputRate(unsigned rate, double speed)
{
	const auto previous = getEmuClock();
	ResampledSoundDevice::setOutputRate(rate, speed);
	// A newly constructed clock has period zero, so it cannot match
	// a real sample period. No separate initialization flag is needed.
	if (previous.getPeriod() == getEmuClock().getPeriod()) {
		getEmuClock().reset(previous.getTime());
	}
}

void YM2608::FmPart::generateChannels(std::span<float*> buffers, unsigned num)
{
	assert(buffers.size() == 13);
	chip.generateFM(buffers, num);
}


YM2608::Timer::Timer(Scheduler& scheduler_, YM2608& ym2608_, uint8_t index_)
	: Schedulable(scheduler_)
	, ym2608(ym2608_)
	, index(index_)
{
}

void YM2608::Timer::cancel()
{
	removeSyncPoints();
}

void YM2608::Timer::schedule(EmuTime time)
{
	cancel();
	setSyncPoint(time);
}

void YM2608::Timer::executeUntil(EmuTime time)
{
	ym2608.updateStream(time);
	// YMFM reloads the timer through ymfm_set_timer().
	ym2608.fm.engine_timer_expired(index);
}

template<typename Archive>
void YM2608::Timer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
}


YM2608::Registers::Registers(MSXMotherBoard& board, std::string_view name_, YM2608& ym2608_)
	: SimpleDebuggable(board, strCat(name_, " registers"), "Effective YM2608 core registers", 512)
	, ym2608(ym2608_)
{
}

uint8_t YM2608::Registers::read(unsigned address, EmuTime time)
{
	return ym2608.peekRegister(address, time);
}

void YM2608::Registers::write(unsigned address, uint8_t value, EmuTime time)
{
	ym2608.writeRegister(address, value, time);
}

} // namespace openmsx
