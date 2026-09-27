#include "MSXMakoto.hh"

#include "DeviceConfig.hh"
#include "ResampledSoundDevice.hh"
#include "Schedulable.hh"
#include "IRQHelper.hh"
#include "IntegerSetting.hh"
#include "Rom.hh"
#include "SimpleDebuggable.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "serialize_stl.hh"
#include "3rdparty/ymfm/ymfm_opn.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <optional>
#include <vector>

namespace openmsx {
// Cartridge integration is separate from the pinned YMFM core (see README.openmsx).
class MakotoSound final : public ResampledSoundDevice,
			  private Schedulable,
			  private ymfm::ymfm_interface
{
public:
	MakotoSound(DeviceConfig& config, EmuTime time)
		: ResampledSoundDevice(config.getMotherBoard(), "Makoto", "Makoto YM2608 OPNA", 16,
				       unsigned(config.getChildDataAsInt("clock", 8000000)) / 8,
				       true)
		, Schedulable(config.getScheduler())
		, irq(config.getMotherBoard(), "Makoto.IRQ", config)
		, psgVolume(config.getCommandController(), "makoto_psg_volume",
			    "Makoto SSG gain relative to its hardware maximum (linear percent)", 50,
			    0, 100)
		, clock(unsigned(config.getChildDataAsInt("clock", 8000000)))
		, contextTime(time)
		, busyEnd(time)
		, chip(*this)
		, registers(config.getMotherBoard(), *this)
	{
		if (clock < 1000000 || clock > 16000000)
			throw MSXException("Invalid Makoto clock");
		if (config.findChild("rom")) {
			rhythm = std::make_unique<Rom>("Makoto rhythm ROM",
						       "YM2608 internal rhythm samples", config);
			if (rhythm->size() != 8192)
				throw MSXException("Makoto rhythm ROM must be 8192 bytes");
		}
		auto filename = config.getChildData("tracefile", "");
		if (!filename.empty()) {
			trace.open(std::string(filename));
			if (!trace)
				throw MSXException("Cannot open Makoto trace file");
			trace << "ticks,event,address,value\n";
		}
		chip.set_channel_output(channelOutput.data());
		ssgGain = (1.0f / 4.3f) * float(psgVolume.getInt()) / 100.0f;
		psgVolume.attach(*this);
		// 100k feedback resistor in parallel with 47 pF: tau = 4.7 us.
		filterAlpha = float(-std::expm1(-8.0 / (double(clock) * 100000.0 * 47e-12)));
		for (unsigned i = 0; i < filterDecay.size(); ++i) {
			filterDecay[i] = std::pow(1.0f - filterAlpha, float(i));
		}
		reset(time);
		registerSound(config);
	}
	~MakotoSound()
	{
		psgVolume.detach(*this);
		unregisterSound();
		removeSyncPoints();
	}
	void reset(EmuTime time)
	{
		updateStream(time);
		contextTime = time;
		deadlines.fill(EmuTime::infinity());
		removeSyncPoints();
		chip.reset();
		busyEnd = time;
		latch = 0;
		regs.fill(0);
		channelOutput.fill(0);
		filterState.fill(0.0f);
		irq.reset();
		log("reset", 0, 0);
	}
	byte read(unsigned port, EmuTime time)
	{
		updateStream(time);
		contextTime = time;
		auto value = chip.read(port);
		log("read", port, value);
		return value;
	}
	byte peek(unsigned port, EmuTime time)
	{
		// Debugger reads must not advance ADPCM RAM, audio, flags or timers.
		if (port == 0 || port == 2) {
			auto savedTime = contextTime;
			contextTime = time;
			std::vector<uint8_t> state;
			ymfm::ymfm_saved_state saved(state, true);
			chip.save_restore(saved);
			suppressIRQ = true;
			auto result = port == 0 ? chip.read_status() : chip.read_status_hi();
			ymfm::ymfm_saved_state restore(state, false);
			chip.save_restore(restore);
			suppressIRQ = false;
			contextTime = savedTime;
			return result;
		}
		return (port == 1 && latch < 14) ? regs[latch] : 0xff;
	}
	void write(unsigned port, byte value, EmuTime time)
	{
		updateStream(time);
		contextTime = time;
		if (!(port & 1))
			latch = uint16_t(value | ((port & 2) << 7));
		else if (unsigned(latch >> 8) == (port >> 1)) {
			regs[latch] = value;
			log("write", latch, value);
		}
		chip.write(port, value);
		reschedule();
	}
	template<typename Archive> void serialize(Archive& ar, unsigned version)
	{
		if constexpr (!Archive::IS_LOADER)
			updateStream(Schedulable::getCurrentTime());
		// Format 1 is the pinned YMFM revision in README.openmsx. A future core
		// upgrade must retain a decoder/migration for this format, even at equal size.
		unsigned coreFormat = 1;
		if (ar.versionAtLeast(version, 2))
			ar.serialize("coreFormat", coreFormat);
		if (coreFormat != 1)
			throw MSXException("Unsupported Makoto core state format");
		std::vector<uint8_t> state;
		if constexpr (!Archive::IS_LOADER) {
			ymfm::ymfm_saved_state saved(state, true);
			chip.save_restore(saved);
		}
		ar.serialize("core", state, "deadlines", deadlines, "busyEnd", busyEnd, "regs",
			     regs, "latch", latch, "sampleRAM", sampleRAM, "irq", irq,
			     "sampleClock", getEmuClock());
		if (ar.versionAtLeast(version, 2)) {
			ar.serialize("channelOutput", channelOutput, "filterState", filterState);
		} else if constexpr (Archive::IS_LOADER) {
			// Legacy states did not retain separated voices or analogue filter history.
			// Start those caches silent; voices refresh on the next FM clock (<18 us).
			channelOutput.fill(0);
			filterState.fill(0.0f);
		}
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
			contextTime = Schedulable::getCurrentTime();
			reschedule();
		}
	}

private:
	void log(const char* event, unsigned address, unsigned value)
	{
		if (trace.is_open())
			trace << (contextTime - EmuTime::zero()).toUint64() << ',' << event << ','
			      << address << ',' << value << '\n';
	}
	EmuDuration clocks(unsigned count) const
	{
		return EmuDuration::sec(double(count) / clock);
	}
	void reschedule()
	{
		removeSyncPoints();
		std::optional<EmuTime> next;
		for (auto deadline : deadlines)
			if (deadline != EmuTime::infinity() && (!next || deadline < *next))
				next = deadline;
		if (next)
			setSyncPoint(*next);
	}
	void executeUntil(EmuTime time) override
	{
		updateStream(time);
		contextTime = time;
		for (unsigned i = 0; i < 2; ++i) {
			if (deadlines[i] <= time) {
				deadlines[i] = EmuTime::infinity();
				log("timer", i, 1);
				m_engine->engine_timer_expired(i);
			}
		}
		reschedule();
	}
	void ymfm_set_timer(uint32_t timer, int32_t duration) override
	{
		if (duration < 0)
			deadlines[timer] = EmuTime::infinity();
		else
			deadlines[timer] = contextTime + clocks(unsigned(duration));
	}
	void ymfm_set_busy_end(uint32_t duration) override
	{
		busyEnd = contextTime + clocks(duration);
	}
	bool ymfm_is_busy() override
	{
		return contextTime < busyEnd;
	}
	void ymfm_update_irq(bool asserted) override
	{
		if (!suppressIRQ) {
			irq.set(asserted);
			log("irq", 0, asserted);
		}
	}
	uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override
	{
		if (type == ymfm::ACCESS_ADPCM_B)
			return sampleRAM[address & 0x3ffff];
		if (type == ymfm::ACCESS_ADPCM_A)
			return rhythm ? (*rhythm)[address & 0x1fff] : 0;
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
			updateStream(Schedulable::getCurrentTime());
			ssgGain = (1.0f / 4.3f) * float(psgVolume.getInt()) / 100.0f;
		} else {
			ResampledSoundDevice::update(setting);
		}
	}
	void makeTargets(const ymfm::ym2608::output_data& mixed, std::array<float, 32>& targets)
	{
		const auto& ssg = chip.ssg_output();
		// MAX fidelity repeats samples. Assign rounding to an active SSG voice.
		int ssg0 = ssg.data[0] * 2 / 3;
		int ssg1 = ssg.data[1] * 2 / 3;
		int ssg2 = ssg.data[2] * 2 / 3;
		int remainder = mixed.data[2] - ssg0 - ssg1 - ssg2;
		if (ssg.data[2] != 0)
			ssg2 += remainder;
		else if (ssg.data[1] != 0)
			ssg1 += remainder;
		else
			ssg0 += remainder;
		for (unsigned side = 0; side < 2; ++side) {
			channelOutput[12 + side] = ssg0;
			channelOutput[14 + side] = ssg1;
			channelOutput[16 + side] = ssg2;
			int total = 0;
			for (unsigned c = 0; c < 16; ++c) {
				if (c < 6 || c >= 9)
					total += channelOutput[2 * c + side];
			}
			// Preserve the shared DAC clamp before the analogue summer.
			float dacScale = total ? float(mixed.data[side]) / float(total) : 1.0f;
			for (unsigned c = 0; c < 16; ++c) {
				float gain = (c >= 6 && c < 9) ? ssgGain : dacScale;
				targets[2 * c + side] = float(channelOutput[2 * c + side]) * gain;
			}
		}
	}

	void advanceFilters(const std::array<float, 32>& targets, unsigned count)
	{
		if (count == 0)
			return;
		float decay = count < filterDecay.size()
				  ? filterDecay[count]
				  : std::pow(1.0f - filterAlpha, float(count));
		for (unsigned c = 0; c < 32; ++c) {
			auto& filtered = filterState[c];
			if (targets[c] == 0.0f && filtered == 0.0f)
				continue;
			// Exact recurrence for a constant input over count chip samples.
			filtered = targets[c] + (filtered - targets[c]) * decay;
			if (targets[c] == 0.0f && std::abs(filtered) < 1e-10f)
				filtered = 0.0f;
		}
	}

	void generateChannels(std::span<float*> buffers, unsigned num) override
	{
		std::array<float, 32> targets = {};
		// Usually openMSX asks for the combined output. Filter that sum at
		// 1 MHz, and advance individual histories only when an input changes.
		// This preserves histories when the channel viewer/mutes are enabled.
		if (std::ranges::all_of(buffers, [&](auto* b) { return b == buffers[0]; })) {
			std::array<float, 2> combined = {};
			for (unsigned c = 0; c < 32; ++c)
				combined[c & 1] += filterState[c];
			std::array<int32_t, 3> lastSSG = {};
			unsigned pending = 0;
			bool audible = false;
			for (unsigned i = 0; i < num; ++i) {
				ymfm::ym2608::output_data mixed;
				chip.generate(&mixed);
				const auto& ssg = chip.ssg_output();
				bool changed = i == 0 || chip.channel_output_changed();
				for (unsigned c = 0; c < 3; ++c) {
					changed = changed || lastSSG[c] != ssg.data[c];
					lastSSG[c] = ssg.data[c];
				}
				if (changed) {
					advanceFilters(targets, pending);
					makeTargets(mixed, targets);
					pending = 0;
				}
				++pending;
				for (unsigned side = 0; side < 2; ++side) {
					float target = float(mixed.data[side]) +
						       ssgGain * float(mixed.data[2]);
					combined[side] += filterAlpha * (target - combined[side]);
					if (target == 0.0f && std::abs(combined[side]) < 1e-10f)
						combined[side] = 0.0f;
					buffers[0][2 * i + side] += combined[side];
					audible = audible || combined[side] != 0.0f;
				}
			}
			advanceFilters(targets, pending);
			for (unsigned c = 1; c < 16; ++c)
				buffers[c] = nullptr;
			if (!audible)
				buffers[0] = nullptr;
			return;
		}

		std::array<bool, 16> audible = {};
		for (unsigned i = 0; i < num; ++i) {
			ymfm::ym2608::output_data mixed;
			chip.generate(&mixed);
			makeTargets(mixed, targets);
			for (unsigned c = 0; c < 16; ++c) {
				for (unsigned side = 0; side < 2; ++side) {
					auto index = 2 * c + side;
					auto& filtered = filterState[index];
					float target = targets[index];
					if (target == 0.0f && filtered == 0.0f)
						continue;
					filtered += filterAlpha * (target - filtered);
					if (target == 0.0f && std::abs(filtered) < 1e-10f)
						filtered = 0.0f;
					buffers[c][2 * i + side] += filtered;
					audible[c] = audible[c] || filtered != 0.0f;
				}
			}
		}
		// Keep clocking envelopes/noise/ADPCM even when all output is silent.
		for (unsigned c = 0; c < 16; ++c) {
			if (!audible[c])
				buffers[c] = nullptr;
		}
	}

	struct Registers final : SimpleDebuggable {
		Registers(MSXMotherBoard& board, MakotoSound& owner_)
			: SimpleDebuggable(board, "Makoto registers", "Last YM2608 register writes",
					   512)
			, owner(owner_)
		{
		}
		byte read(unsigned address) override
		{
			return owner.regs[address];
		}
		MakotoSound& owner;
	};
	std::array<int32_t, 32> channelOutput = {};
	std::array<float, 32> filterState = {};
	float filterAlpha = 0.0f;
	std::array<float, 19> filterDecay = {};
	float ssgGain = 0.0f;
	bool sampleClockInitialized = false;
	bool suppressIRQ = false;
	OptionalIRQHelper irq;
	IntegerSetting psgVolume;
	const unsigned clock;
	EmuTime contextTime;
	EmuTime busyEnd;
	std::array<EmuTime, 2> deadlines = {EmuTime::infinity(), EmuTime::infinity()};
	ymfm::ym2608 chip;
	std::array<byte, 512> regs = {};
	uint16_t latch = 0;
	std::array<byte, 262144> sampleRAM = {};
	std::unique_ptr<Rom> rhythm;
	std::ofstream trace;
	Registers registers;
};

SERIALIZE_CLASS_VERSION(MakotoSound, 2);

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
