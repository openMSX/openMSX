#include "MSXMakoto.hh"
#include "MakotoYM2608.hh"

#include "DeviceConfig.hh"
#include "Clock.hh"
#include "ResampledSoundDevice.hh"
#include "Schedulable.hh"
#include "IRQHelper.hh"
#include "Ram.hh"
#include "MSXException.hh"
#include "SimpleDebuggable.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "serialize_stl.hh"
#include "3rdparty/ymfm/ymfm_opn.h"
#include "3rdparty/ym2608/fmopn_2608rom.h"

#include <algorithm>
#include <array>
#include <vector>
#include <numbers>

namespace openmsx {
// Cartridge integration is separate from the pinned YMFM core (see README.openmsx).
class MakotoSound final : private ymfm::ymfm_interface
{
	static constexpr unsigned CLOCK = 8000000;

	// Both hardware streams have symmetric ownership and independent clocks.
	class Part : public ResampledSoundDevice {
	public:
		Part(DeviceConfig& config, std::string_view name, static_string_view description,
		     unsigned channels, unsigned rate, bool stereo)
			: ResampledSoundDevice(config.getMotherBoard(), name, description, channels, rate, stereo) {}
		void start(DeviceConfig& config) { registerSound(config); registered = true; }
		~Part() { if (registered) unregisterSound(); }
		void sync(EmuTime time) { updateStream(time); }
		void restoreClock(EmuTime time) { createResampler(); getEmuClock().reset(time); }
		void rate(unsigned value) {
			if (getInputRate() != value) { setInputRate(value); createResampler(); }
		}
		void setOutputRate(unsigned rate, double speed) override {
			const auto previous = getEmuClock();
			ResampledSoundDevice::setOutputRate(rate, speed);
			if (clockInitialized && previous.getPeriod() == getEmuClock().getPeriod())
				getEmuClock().reset(previous.getTime());
			clockInitialized = true;
		}
	private:
		bool registered = false;
		bool clockInitialized = false;
	};

	class FmPart final : public Part {
	public:
		FmPart(DeviceConfig& config, std::string_view name, MakotoYM2608& chip_)
			: Part(config, name, "Makoto FM, rhythm and ADPCM", 13, (CLOCK + 72) / 144, true)
			, chip(chip_) {}
	private:
		void generateChannels(std::span<float*> buffers, unsigned num) override {
			chip.generateFM(buffers, num);
		}
		MakotoYM2608& chip;
	};

	class SsgPart final : public Part {
	public:
		SsgPart(DeviceConfig& config, std::string_view name, MakotoYM2608& chip_)
			: Part(config, strCat(name, " SSG"), "Makoto SSG", 3, CLOCK / 32, false)
			, chip(chip_) {}
	private:
		float getAmplificationFactorImpl() const override {
			// Maximum normalization. Standard SSG volume replaces the old trim.
			return std::numbers::sqrt2_v<float> * (2.0f / 3.0f) / (32768.0f * 4.3f);
		}
		void generateChannels(std::span<float*> buffers, unsigned num) override {
			chip.generateSSG(buffers, num);
		}
		MakotoYM2608& chip;
	};

	class Timer final : public Schedulable {
	public:
		Timer(Scheduler& scheduler, MakotoSound& owner_, unsigned index_)
			: Schedulable(scheduler), owner(owner_), index(index_) {}

		void cancel() { removeSyncPoints(); }
		void schedule(EmuTime time) {
			cancel();
			setSyncPoint(time);
		}
		template<typename Archive>
		void serialize(Archive& ar, unsigned /*version*/) {
			ar.template serializeBase<Schedulable>(*this);
		}

	private:
		void executeUntil(EmuTime time) override {
			owner.updateStream(time);
			owner.contextTime = time;
			// YMFM reloads the timer through ymfm_set_timer().
			owner.m_engine->engine_timer_expired(index);
		}

		MakotoSound& owner;
		const unsigned index;
	};

public:
	MakotoSound(DeviceConfig& config, std::string_view name, EmuTime time)
		: irq(config.getMotherBoard(), strCat(name, ".IRQ"))
		, timers{Timer(config.getScheduler(), *this, 0),
			 Timer(config.getScheduler(), *this, 1)}
		, contextTime(time)
		, busyEnd(time)
		, chip(*this)
		, sampleRAM(config, strCat(name, " ADPCM RAM"), "YM2608 ADPCM-B sample RAM", 262144)
		, registers(config.getMotherBoard(), name, *this)
		, fmPart(config, name, chip)
		, ssgPart(config, name, chip)
	{
		sampleRAM.clear(0); // Deterministic emulator policy; hardware power-on contents are unknown.
		reset(time);
		fmPart.start(config);
		ssgPart.start(config);
	}

	void reset(EmuTime time)
	{
		updateStream(time);
		contextTime = time;
		for (auto& timer : timers) timer.cancel();
		chip.reset();
		applyRates();
		busyEnd = time;
		irq.reset();
	}
	byte read(unsigned port, EmuTime time)
	{
		updateStream(time);
		contextTime = time;
		return chip.read(port);
	}
	byte peek(unsigned port, EmuTime time)
	{
		return chip.peek(port, time < busyEnd);
	}
	void write(unsigned port, byte value, EmuTime time)
	{
		updateStream(time);
		contextTime = time;
		chip.write(port, value);
		applyRates();
	}
	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		if constexpr (!Archive::IS_LOADER)
			updateStream(timers[0].getCurrentTime());
		ar.serialize("timerA", timers[0], "timerB", timers[1]);
		std::vector<uint8_t> state;
		if constexpr (!Archive::IS_LOADER) {
			ymfm::ymfm_saved_state saved(state, true);
			chip.save_restore(saved);
		}
		ar.serialize("core", state, "busyEnd", busyEnd,
			     "sampleRAM", sampleRAM, "irq", irq, "sampleClock", fmPart.getEmuClock(),
			     "ssgClock", ssgPart.getEmuClock());
		if constexpr (Archive::IS_LOADER) {
			// Reject truncated core state instead of allowing YMFM's zero-fill
			// fallback.
			std::vector<uint8_t> expected;
			ymfm::ymfm_saved_state measure(expected, true);
			chip.save_restore(measure);
			if (state.size() != expected.size())
				throw MSXException("Invalid Makoto core state size");
			ymfm::ymfm_saved_state saved(state, false);
			chip.save_restore(saved);
			const auto fmTime = fmPart.getEmuClock().getTime();
			const auto ssgTime = ssgPart.getEmuClock().getTime();
			applyRates();
			ssgPart.restoreClock(ssgTime);
			fmPart.restoreClock(fmTime);
			chip.invalidate_caches();
			contextTime = timers[0].getCurrentTime();
		}
	}

private:
	void ymfm_set_timer(uint32_t timer, int32_t duration) override
	{
		if (duration < 0)
			timers[timer].cancel();
		else
			timers[timer].schedule(contextTime + Clock<CLOCK>::duration(unsigned(duration)));
	}
	void ymfm_set_busy_end(uint32_t duration) override
	{
		busyEnd = contextTime + Clock<CLOCK>::duration(duration);
	}
	bool ymfm_is_busy() override
	{
		return contextTime < busyEnd;
	}
	void ymfm_update_irq(bool asserted) override
	{
		irq.set(asserted);
	}
	uint8_t ymfm_external_peek(ymfm::access_class type, uint32_t address) override
	{
		// Both sample stores are passive memory; GPIO is unconnected.
		return ymfm_external_read(type, address);
	}
	uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override
	{
		if (type == ymfm::ACCESS_ADPCM_B)
			return sampleRAM[address & 0x3ffff];
		if (type == ymfm::ACCESS_ADPCM_A)
			return YM2608_ADPCM_ROM[address & 0x1fff];
		return 0xff; // SSG GPIO is not attached to the MSX keyboard or joysticks.
	}
	void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t value) override
	{
		if (type == ymfm::ACCESS_ADPCM_B)
			sampleRAM[address & 0x3ffff] = value;
	}
	void updateStream(EmuTime time) { fmPart.sync(time); }
	void applyRates() {
		fmPart.rate(chip.fmRate());
		ssgPart.rate(chip.ssgRate());
	}

	struct Registers final : SimpleDebuggable {
		Registers(MSXMotherBoard& board, std::string_view name, MakotoSound& owner_)
			: SimpleDebuggable(board, strCat(name, " registers"), "Effective YM2608 core registers",
					   512)
			, owner(owner_)
		{
		}
		byte read(unsigned address) override
		{
			return owner.chip.peek_register(uint16_t(address));
		}
		void write(unsigned address, byte value, EmuTime time) override
		{
			owner.updateStream(time);
			owner.contextTime = time;
			owner.chip.write_register(uint16_t(address), value);
			owner.applyRates();
		}
		MakotoSound& owner;
	};
	IRQHelper irq;
	std::array<Timer, 2> timers;
	EmuTime contextTime;
	EmuTime busyEnd;
	MakotoYM2608 chip;
	Ram sampleRAM;
	Registers registers;
	FmPart fmPart;
	SsgPart ssgPart;
};

MSXMakoto::MSXMakoto(DeviceConfig& config)
	: MSXDevice(config)
	, sound(std::make_unique<MakotoSound>(config, getName(), getCurrentTime()))
{
}
MSXMakoto::~MSXMakoto() = default;
void MSXMakoto::reset(EmuTime time)
{
	sound->reset(time);
}
byte MSXMakoto::readIO(uint16_t port, EmuTime time)
{
	return sound->read(port & 3, time);
}
byte MSXMakoto::peekIO(uint16_t port, EmuTime time) const
{
	return sound->peek(port & 3, time);
}
void MSXMakoto::writeIO(uint16_t port, byte value, EmuTime time)
{
	sound->write(port & 3, value, time);
}
template<typename Archive> void MSXMakoto::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("sound", *sound);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMakoto);
REGISTER_MSXDEVICE(MSXMakoto, "Makoto");
} // namespace openmsx
