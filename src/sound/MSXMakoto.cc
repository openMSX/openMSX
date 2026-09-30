#include "MSXMakoto.hh"
#include "MakotoMix.hh"

#include "DeviceConfig.hh"
#include "Clock.hh"
#include "ResampledSoundDevice.hh"
#include "Schedulable.hh"
#include "IRQHelper.hh"
#include "IntegerSetting.hh"
#include "Ram.hh"
#include "MSXException.hh"
#include "SimpleDebuggable.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "serialize_stl.hh"
#include "3rdparty/ymfm/ymfm_opn.h"
#include "3rdparty/ym2608/fmopn_2608rom.h"

#include <algorithm>
#include <array>
#include <vector>

namespace openmsx {
// Cartridge integration is separate from the pinned YMFM core (see README.openmsx).
class MakotoSound final : public ResampledSoundDevice,
			  private ymfm::ymfm_interface
{
	static constexpr unsigned CLOCK = 8000000;

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
	MakotoSound(DeviceConfig& config, EmuTime time)
		: ResampledSoundDevice(config.getMotherBoard(), "Makoto", "Makoto YM2608 OPNA", 16,
				       CLOCK / 8,
				       true)
		, irq(config.getMotherBoard(), "Makoto.IRQ")
		, psgVolume(config.getCommandController(), "makoto_psg_volume",
			    "Makoto SSG gain relative to its hardware maximum (linear percent)", 50,
			    0, 100)
		, timers{Timer(config.getScheduler(), *this, 0),
			 Timer(config.getScheduler(), *this, 1)}
		, contextTime(time)
		, busyEnd(time)
		, chip(*this)
		, sampleRAM(config, "Makoto ADPCM RAM", "YM2608 ADPCM-B sample RAM", 262144)
		, registers(config.getMotherBoard(), *this)
	{
		chip.set_fidelity(ymfm::OPN_FIDELITY_MAX);
		sampleRAM.clear(0); // Start zeroed; reset preserves these contents.
		chip.set_channel_output(channelOutput.data());
		ssgGain = (1.0f / 4.3f) * float(psgVolume.getInt()) / 100.0f;
		psgVolume.attach(*this);
		reset(time);
		registerSound(config);
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
			     "sampleRAM", sampleRAM, "irq", irq, "sampleClock", getEmuClock());
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
			chip.set_fidelity(ymfm::OPN_FIDELITY_MAX);
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
			updateStream(timers[0].getCurrentTime());
			ssgGain = (1.0f / 4.3f) * float(psgVolume.getInt()) / 100.0f;
		} else {
			ResampledSoundDevice::update(setting);
		}
	}
	void generateChannels(std::span<float*> buffers, unsigned num) override
	{
		// YMFM already applies the aggregate DAC clamp and SSG 2/3 scale.
		// Keep its combined output for normal playback. MakotoMix splits
		// that same signal for channel tools (see makoto-mix-test.cc).
		if (std::ranges::all_of(buffers, [&](auto* b) { return b == buffers[0]; })) {
			uint32_t orOutput = 0;
			for (unsigned i = 0; i < num; ++i) {
				ymfm::ym2608::output_data output;
				chip.generate(&output);
				float ssg = ssgGain * float(output.data[2]);
				buffers[0][2 * i]     += float(output.data[0]) + ssg;
				buffers[0][2 * i + 1] += float(output.data[1]) + ssg;
				orOutput |= uint32_t(output.data[0] | output.data[1] | output.data[2]);
			}
			std::ranges::fill(buffers.subspan(1), nullptr);
			// A muted but active SSG conservatively keeps this buffer active.
			if (orOutput == 0) buffers[0] = nullptr;
			return;
		}
		MakotoMix mix;
		std::array<int32_t, 3> lastSSG = {};
		for (unsigned i = 0; i < num; ++i) {
			ymfm::ym2608::output_data output;
			chip.generate(&output);
			const auto& ssg = chip.ssg_output();
			bool changed = i == 0 || chip.channel_output_changed();
			for (unsigned c = 0; c < 3; ++c) {
				changed = changed || lastSSG[c] != ssg.data[c];
				lastSSG[c] = ssg.data[c];
			}
			if (changed) {
				mix.update(channelOutput, ssg.data, output.data, ssgGain);
			}
			for (unsigned c = 0; c < 16; ++c) {
				buffers[c][2 * i]     += mix.voices[2 * c];
				buffers[c][2 * i + 1] += mix.voices[2 * c + 1];
			}
		}
		// Channel tools usually inspect active output. Avoid a per-voice
		// silence scan; the host can sum these buffers directly.
	}

	struct Registers final : SimpleDebuggable {
		Registers(MSXMotherBoard& board, MakotoSound& owner_)
			: SimpleDebuggable(board, "Makoto registers", "Effective YM2608 core registers",
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
		}
		MakotoSound& owner;
	};
	std::array<int32_t, 32> channelOutput = {};
	float ssgGain = 0.0f;
	bool sampleClockInitialized = false;
	IRQHelper irq;
	IntegerSetting psgVolume;
	std::array<Timer, 2> timers;
	EmuTime contextTime;
	EmuTime busyEnd;
	ymfm::ym2608 chip;
	Ram sampleRAM;
	Registers registers;
};

// The initial upstream state format is version 1. Fork migration stays in the fork.

MSXMakoto::MSXMakoto(DeviceConfig& config)
	: MSXDevice(config)
	, sound(std::make_unique<MakotoSound>(config, getCurrentTime()))
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
