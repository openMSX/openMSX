#include "SN76489.hh"
#include "DeviceConfig.hh"
#include "Math.hh"
#include "cstd.hh"
#include "one_of.hh"
#include "outer.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

using std::string;

namespace openmsx {

// The SN76489 divides the clock input by 8, but all users of the clock apply
// another divider of 2.
constexpr auto NATIVE_FREQ_INT = unsigned(cstd::round((3579545.0 / 8) / 2));

static constexpr auto volTable = [] {
	std::array<unsigned, 16> result = {};
	// 2dB per step -> 0.2, sqrt for amplitude -> 0.5
	double factor = cstd::pow<5, 3>(0.1, 0.2 * 0.5);
	double out = 32768.0;
	for (auto i : xrange(15)) {
		result[i] = cstd::round(out);
		out *= factor;
	}
	result[15] = 0;
	return result;
}();


// NoiseShifter:

inline void SN76489::NoiseShifter::initState(unsigned pattern_,
                                             unsigned period_)
{
	pattern = pattern_;
	period = period_;
	stepsBehind = 0;

	// Start with only the top bit set.
	unsigned allOnes = Math::floodRight(pattern_);
	random = allOnes - (allOnes >> 1);
}

inline unsigned SN76489::NoiseShifter::getOutput() const
{
	return ~random & 1;
}

inline void SN76489::NoiseShifter::advance()
{
	random = (random >> 1) ^ ((random & 1) ? pattern : 0);
}

inline void SN76489::NoiseShifter::queueAdvance(unsigned steps)
{
	stepsBehind += steps;
	stepsBehind %= period;
}

void SN76489::NoiseShifter::catchUp()
{
	for (/**/; stepsBehind; stepsBehind--) {
		advance();
	}
}

template<typename Archive>
void SN76489::NoiseShifter::serialize(Archive& ar, unsigned /*version*/)
{
	// Make sure there are no queued steps, so we don't have to serialize them.
	// If we're loading, initState() already set stepsBehind to 0.
	if constexpr (!ar.IS_LOADER) {
		catchUp();
	}
	assert(stepsBehind == 0);

	// Don't serialize the pattern and noise period; they are initialized by
	// the chip class.
	ar.serialize("random", random);
}

// Main class:

SN76489::SN76489(const DeviceConfig& config)
	: ResampledSoundDevice(config.getMotherBoard(), "SN76489", "DCSG", 4, NATIVE_FREQ_INT, false)
	, debuggable(config.getMotherBoard(), getName())
{
	if (false) {
		std::cout << "volTable:";
		for (const auto& e : volTable) std::cout << ' ' << e;
		std::cout << '\n';
	}
	initState();

	registerSound(config);
}

SN76489::~SN76489()
{
	unregisterSound();
}

void SN76489::initState()
{
	registerLatch = 0; // TODO: 3 for Sega.

	// The Sega integrated versions start zeroed (max volume), while discrete
	// chips seem to start with random values (for lack of a reset pin).
	// For the user's comfort, we init to silence instead.
	for (auto chan : xrange(4)) {
		regs[chan * 2 + 0] = 0x0;
		regs[chan * 2 + 1] = 0xF;
	}
	ranges::fill(counters, 0);
	ranges::fill(outputs, 0);

	initNoise();
}

void SN76489::initNoise()
{
	// Note: These are the noise patterns for the SN76489A.
	//       Other chip variants have different noise patterns.
	unsigned period = (regs[6] & 0x4)
	                ? ((1 << 15) - 1) // "White" noise: pseudo-random bit sequence.
	                : 15;          // "Periodic" noise: produces a square wave with a short duty cycle.
	unsigned pattern = (regs[6] & 0x4)
	                 ? 0x6000
	                 : (1 << (period - 1));
	noiseShifter.initState(pattern, period);
}

void SN76489::reset(EmuTime::param time)
{
	updateStream(time);
	initState();
}

void SN76489::write(byte value, EmuTime::param time)
{
	if (value & 0x80) {
		registerLatch = (value & 0x70) >> 4;
	}
	auto& reg = regs[registerLatch];

	word data = [&] {
		switch (registerLatch) {
		case 0:
		case 2:
		case 4:
			// Tone period.
			if (value & 0x80) {
				return (reg & 0x3F0) | (value & 0x0F);
			}  else {
				return (reg & 0x00F) | ((value & 0x3F) << 4);
			}
		case 6:
			// Noise control.
			return value & 0x07;
		case 1:
		case 3:
		case 5:
		case 7:
			// Attenuation.
			return value & 0x0F;
		default:
			UNREACHABLE; return 0;
		}
	}();

	// TODO: Warn about access while chip is not ready.

	writeRegister(registerLatch, data, time);
}

word SN76489::peekRegister(unsigned reg, EmuTime::param /*time*/) const
{
	// Note: None of the register values will change unless a register is
	//       written, so we don't need to sync here.
	return regs[reg];
}

void SN76489::writeRegister(unsigned reg, word value, EmuTime::param time)
{
	if (reg == 6 || regs[reg] != value) {
		updateStream(time);
		regs[reg] = value;
		if (reg == 6) {
			// Every write to register 6 resets the noise shift register.
			initNoise();
		}
	}
}

/*
 * Implementation note for noise mode 3:
 *
 * In this mode, the shift register is advanced when the tone generator #3
 * output flips to 1. We emulate this by synthesizing the noise channel using
 * that generator before synthesizing its tone channel, but without committing
 * the updated state. This ensures that the noise channel and the third tone
 * channel are in phase, but do end up in their own separate mixing buffers.
 */

template<bool NOISE> void SN76489::synthesizeChannel(
		float*& buffer, unsigned num, unsigned generator)
{
	unsigned period = [&] {
		if (generator == 3) {
			// Noise channel using its own generator.
			return unsigned(16 << (regs[6] & 3));
		} else {
			// Tone or noise channel using a tone generator.
			unsigned p = regs[2 * generator];
			if (p == 0) {
				// TODO: Sega variants have a non-flipping output for period 0.
				p = 0x400;
			}
			return p;
		}
	}();

	auto output = outputs[generator];
	unsigned counter = counters[generator];

	unsigned channel = NOISE ? 3 : generator;
	int volume = volTable[regs[2 * channel + 1]];
	if (volume == 0) {
		// Channel is silent, don't synthesize it.
		buffer = nullptr;
	}
	if (buffer) {
		// Synthesize channel.
		if constexpr (NOISE) {
			noiseShifter.catchUp();
		}
		auto* buf = buffer;
		unsigned remaining = num;
		while (remaining != 0) {
			if (counter == 0) {
				output ^= 1;
				counter = period;
				if (NOISE && output) {
					noiseShifter.advance();
				}
			}
			unsigned ticks = std::min(counter, remaining);
			if (NOISE ? noiseShifter.getOutput() : output) {
				addFill(buf, volume, ticks);
			} else {
				buf += ticks;
			}
			counter -= ticks;
			remaining -= ticks;
		}
	} else {
		// Advance state without synthesis.
		if (counter >= num) {
			counter -= num;
		} else {
			unsigned remaining = num - counter;
			output ^= 1; // partial cycle
			unsigned cycles = (remaining - 1) / period;
			if constexpr (NOISE) {
				noiseShifter.queueAdvance((cycles + output) / 2);
			}
			output ^= cycles & 1; // full cycles
			remaining -= cycles * period;
			counter = period - remaining;
		}
	}

	if (!NOISE || generator == 3) {
		outputs[generator] = output;
		counters[generator] = counter;
	}
}

void SN76489::generateChannels(float** buffers, unsigned num)
{
	// Channel 3: noise.
	if ((regs[6] & 3) == 3) {
		// Use the tone generator #3 (channel 2) output.
		synthesizeChannel<true>(buffers[3], num, 2);
		// Assume the noise phase counter and output bit keep updating even
		// if they are currently not driving the noise shift register.
		float* noBuffer = nullptr;
		synthesizeChannel<false>(noBuffer, num, 3);
	} else {
		// Use the channel 3 generator output.
		synthesizeChannel<true>(buffers[3], num, 3);
	}

	// Channels 0, 1, 2: tone.
	for (auto channel : xrange(3)) {
		synthesizeChannel<false>(buffers[channel], num, channel);
	}
}

template<typename Archive>
void SN76489::serialize(Archive& ar, unsigned version)
{
	ar.serialize("regs",          regs,
	             "registerLatch", registerLatch,
	             "counters",      counters,
	             "outputs",       outputs);

	if constexpr (ar.IS_LOADER) {
		// Initialize the computed NoiseShifter members, based on the
		// register contents we just loaded, before we load the shift
		// register state.
		initNoise();
	}
	// Serialize the noise shift register state as part of the chip: there is
	// no need to reflect our class structure in the serialization.
	noiseShifter.serialize(ar, version);
}
INSTANTIATE_SERIALIZE_METHODS(SN76489);

// Debuggable

// The frequency registers are 10 bits wide, so we have to split them over
// two debuggable entries.
constexpr byte SN76489_DEBUG_MAP[][2] = {
	{0, 0}, {0, 1}, {1, 0},
	{2, 0}, {2, 1}, {3, 0},
	{4, 0}, {4, 1}, {5, 0},
	{6, 0}, {7, 0}
};

SN76489::Debuggable::Debuggable(MSXMotherBoard& motherBoard_, const string& name_)
	: SimpleDebuggable(
		motherBoard_, name_ + " regs",
		"SN76489 regs - note the period regs are split over two entries", 11)
{
}

byte SN76489::Debuggable::read(unsigned address, EmuTime::param time)
{
	byte reg = SN76489_DEBUG_MAP[address][0];
	byte hi = SN76489_DEBUG_MAP[address][1];

	auto& sn76489 = OUTER(SN76489, debuggable);
	word data = sn76489.peekRegister(reg, time);
	return hi ? (data >> 4) : (data & 0xF);
}

void SN76489::Debuggable::write(unsigned address, byte value, EmuTime::param time)
{
	byte reg = SN76489_DEBUG_MAP[address][0];
	byte hi = SN76489_DEBUG_MAP[address][1];

	auto& sn76489 = OUTER(SN76489, debuggable);
	word data = [&] {
		if (reg == one_of(0, 2, 4)) {
			word d = sn76489.peekRegister(reg, time);
			if (hi) {
				return ((value & 0x3F) << 4) | (d & 0x0F);
			} else {
				return (d & 0x3F0) | (value & 0x0F);
			}
		} else {
			return value & 0x0F;
		}
	}();
	sn76489.writeRegister(reg, data, time);
}

} // namespace openmsx
