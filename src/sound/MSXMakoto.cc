#include "MSXMakoto.hh"
#include "MakotoNativeChip.hh"

#include "DeviceConfig.hh"
#include "Clock.hh"
#include "ResampledSoundDevice.hh"
#include "Schedulable.hh"
#include "IRQHelper.hh"
#include "IntegerSetting.hh"
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
class MakotoSound final : public ResampledSoundDevice,
			  private ymfm::ymfm_interface
{
	static constexpr unsigned CLOCK = 8000000;

	class SsgPart final : public ResampledSoundDevice {
	public:
		SsgPart(DeviceConfig& config, std::string_view name, MakotoSound& owner_)
			: ResampledSoundDevice(config.getMotherBoard(), strCat(name, " SSG"), "Makoto SSG", 3, CLOCK / 32, false)
			, owner(owner_) {}
		void start(DeviceConfig& config) { registerSound(config); registered = true; }
		~SsgPart() { if (registered) unregisterSound(); }
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
		float getAmplificationFactorImpl() const override {
			// Mono centre panning contributes 1/sqrt(2) to each host side.
			// Preserve the previous YMFM numeric scale and board balance here,
			// without integer rounding or gain work in the source-sample loop.
			return std::numbers::sqrt2_v<float> * (2.0f / 3.0f) / (32768.0f * 4.3f);
		}
		void generateChannels(std::span<float*> buffers, unsigned num) override {
			const bool combined = std::ranges::all_of(buffers, [&](auto* b) { return b == buffers[0]; });
			uint32_t orOutput = 0;
			for (unsigned i = 0; i < num; ++i) {
				const auto s = owner.chip.clockSSG();
				const auto total = s[0] + s[1] + s[2];
				if (combined) {
					buffers[0][i] += float(total);
					orOutput |= uint32_t(total);
				} else {
					for (unsigned c = 0; c < 3; ++c) buffers[c][i] += float(s[c]);
				}
			}
			if (combined) {
				std::ranges::fill(buffers.subspan(1), nullptr);
				if (!orOutput) buffers[0] = nullptr;
			}
		}
		MakotoSound& owner;
		bool registered = false;
		bool clockInitialized = false;
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
		: ResampledSoundDevice(config.getMotherBoard(), name, "Makoto FM, rhythm and ADPCM", 13,
				       (CLOCK + 72) / 144,
				       true)
		, irq(config.getMotherBoard(), strCat(name, ".IRQ"))
		, psgVolume(config.getCommandController(), strCat(name, "_psg_volume"),
			    "Makoto SSG gain relative to its hardware maximum (linear percent)", 50,
			    0, 100)
		, timers{Timer(config.getScheduler(), *this, 0),
			 Timer(config.getScheduler(), *this, 1)}
		, contextTime(time)
		, busyEnd(time)
		, chip(*this)
		, sampleRAM(config, strCat(name, " ADPCM RAM"), "YM2608 ADPCM-B sample RAM", 262144)
		, registers(config.getMotherBoard(), name, *this)
		, ssgPart(config, name, *this)
	{
		chip.set_fidelity(ymfm::OPN_FIDELITY_MAX);
		sampleRAM.clear(0); // Start zeroed; reset preserves these contents.
		chip.set_channel_output(channelOutput.data());
		psgVolume.attach(*this);
		reset(time);
		registerSound(config);
		ssgPart.start(config);
		ssgPart.setSoftwareVolume(float(psgVolume.getInt()) / 100.0f, time);
	}
	~MakotoSound()
	{
		psgVolume.detach(*this);
		unregisterSound();
	}
	void reset(EmuTime time)
	{
		updateStream(time);
		contextTime = time;
		for (auto& timer : timers) timer.cancel();
		chip.reset();
		applyRates();
		busyEnd = time;
		channelOutput.fill(0);
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
		// BUSY is observed at the requested time without advancing the chip.
		auto savedTime = contextTime;
		contextTime = time;
		auto result = chip.peek(port);
		contextTime = savedTime;
		return result;
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
			     "sampleRAM", sampleRAM, "irq", irq, "sampleClock", getEmuClock(),
			     "ssgClock", ssgPart.getEmuClock());
		ar.serialize("channelOutput", channelOutput);
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
			const auto fmTime = getEmuClock().getTime();
			const auto ssgTime = ssgPart.getEmuClock().getTime();
			applyRates();
			createResampler();
			ssgPart.restoreClock(ssgTime);
			getEmuClock().reset(fmTime);
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
	void setOutputRate(unsigned hostSampleRate, double speed) override
	{
		const auto previousClock = getEmuClock();
		ResampledSoundDevice::setOutputRate(hostSampleRate, speed);
		if (sampleClockInitialized &&
		    previousClock.getPeriod() == getEmuClock().getPeriod()) {
			getEmuClock().reset(previousClock.getTime());
		}
		sampleClockInitialized = true;
	}
	void update(const Setting& setting) noexcept override
	{
		if (&setting == &psgVolume) {
			// Render up to the change with the previous gain.
			ssgPart.setSoftwareVolume(float(psgVolume.getInt()) / 100.0f, timers[0].getCurrentTime());
		} else {
			ResampledSoundDevice::update(setting);
		}
	}

	void applyRates() {
		if (getInputRate() != chip.fmRate()) {
			setInputRate(chip.fmRate());
			createResampler();
		}
		ssgPart.rate(chip.ssgRate());
	}
	void generateChannels(std::span<float*> buffers, unsigned num) override
	{
		const bool combined = std::ranges::all_of(buffers, [&](auto* b) { return b == buffers[0]; });
		uint32_t orOutput = 0;
		for (unsigned i = 0; i < num; ++i) {
			const auto fm = chip.clockFM();
			if (combined) {
				buffers[0][2 * i] += float(fm[0]);
				buffers[0][2 * i + 1] += float(fm[1]);
				orOutput |= uint32_t(fm[0] | fm[1]);
			} else {
				for (unsigned c = 0; c < 13; ++c) {
					const unsigned source = c < 6 ? c : c + 3;
					buffers[c][2 * i] += float(channelOutput[2 * source]);
					buffers[c][2 * i + 1] += float(channelOutput[2 * source + 1]);
				}
			}
		}
		if (combined) {
			std::ranges::fill(buffers.subspan(1), nullptr);
			if (!orOutput) buffers[0] = nullptr;
		}
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
	std::array<int32_t, 32> channelOutput = {};
	bool sampleClockInitialized = false;
	IRQHelper irq;
	IntegerSetting psgVolume;
	std::array<Timer, 2> timers;
	EmuTime contextTime;
	EmuTime busyEnd;
	MakotoNativeChip chip;
	Ram sampleRAM;
	Registers registers;
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
