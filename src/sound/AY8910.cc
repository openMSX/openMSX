/*
 * Emulation of the AY-3-8910
 *
 * Original code taken from xmame-0.37b16.1
 *   Based on various code snippets by Ville Hallik, Michael Cuddy,
 *   Tatsuyuki Satoh, Fabrice Frances, Nicola Salmoria.
 * Integrated into openMSX by ???.
 * Refactored in C++ style by Maarten ter Huurne.
 */

#include "AY8910.hh"
#include "AY8910Periphery.hh"
#include "DeviceConfig.hh"
#include "GlobalSettings.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "cstd.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "outer.hh"
#include "random.hh"
#include "xrange.hh"
#include <array>
#include <cassert>
#include <iostream>

namespace openmsx {

// The step clock for the tone and noise generators is the chip clock
// divided by 8; for the envelope generator of the AY-3-8910, it is half
// that much (clock/16).
static constexpr float NATIVE_FREQ_FLOAT = (3579545.0f / 2) / 8;
static constexpr int NATIVE_FREQ_INT = int(cstd::round(NATIVE_FREQ_FLOAT));

static constexpr int PORT_A_DIRECTION = 0x40;
static constexpr int PORT_B_DIRECTION = 0x80;

static constexpr uint8_t AY_AFINE    =  0;
static constexpr uint8_t AY_ACOARSE  =  1;
static constexpr uint8_t AY_BFINE    =  2;
static constexpr uint8_t AY_BCOARSE  =  3;
static constexpr uint8_t AY_CFINE    =  4;
static constexpr uint8_t AY_CCOARSE  =  5;
static constexpr uint8_t AY_NOISEPER =  6;
static constexpr uint8_t AY_ENABLE   =  7;
static constexpr uint8_t AY_AVOL     =  8;
static constexpr uint8_t AY_BVOL     =  9;
static constexpr uint8_t AY_CVOL     = 10;
static constexpr uint8_t AY_EFINE    = 11;
static constexpr uint8_t AY_ECOARSE  = 12;
static constexpr uint8_t AY_ESHAPE   = 13;
static constexpr uint8_t AY_PORTA    = 14;
static constexpr uint8_t AY_PORTB    = 15;

// Calculate the volume->voltage conversion table. The AY-3-8910 has 16 levels,
// in a logarithmic scale (3dB per step). YM2149 has 32 levels, the 16 extra
// levels are only used for envelope volumes
static constexpr auto YM2149EnvelopeTab = [] {
	std::array<float, 32> result = {};
	double out = 1.0;
	double factor = cstd::pow<5, 3>(0.5, 0.25); // 1/sqrt(sqrt(2)) ~= 1/(1.5dB)
	for (int i = 31; i > 0; --i) {
		result[i] = float(out);
		out *= factor;
	}
	result[0] = 0.0f;
	return result;
}();
static constexpr auto AY8910EnvelopeTab = [] {
	// only 16 envelope steps, duplicate every step
	std::array<float, 32> result = {};
	result[0] = 0.0f;
	result[1] = 0.0f;
	for (int i = 2; i < 32; i += 2) {
		result[i + 0] = YM2149EnvelopeTab[i + 1];
		result[i + 1] = YM2149EnvelopeTab[i + 1];
	}
	return result;
}();
static constexpr auto volumeTab = [] {
	std::array<float, 16> result = {};
	result[0] = 0.0f;
	for (auto i : xrange(1, 16)) {
		result[i] = YM2149EnvelopeTab[2 * i + 1];
	}
	return result;
}();


// 256 random floats, each in range [-1, 1).
//  (with entries 0..2 repeated at entries 256..258).
static constexpr auto noiseTab = []{
	std::array<float, 256 + 3> result = {};
	// Seed doesn't matter. It's OK that these 'random' numbers are the same
	// in each openMSX run.
	PCG<0x123456789abcdefLL> pcg;
	for (int i = 0; i < 256; ++i) {
		result[i] = 2.0f * getCanonicalFloat(pcg()) - 1.0f;
	}
	result[256] = result[0];
	result[257] = result[1];
	result[258] = result[2];
	return result;
}();

static float noiseValue(float x)
{
	// cubic hermite spline interpolation
	assert(0.0f <= x);
	auto xi = int(x);
	auto xf = x - narrow_cast<float>(xi);
	xi &= 255;
	return Math::cubicHermite(subspan<4>(noiseTab, xi), xf);
}


// Generator:

inline void AY8910::Generator::reset()
{
	count = 0;
}

inline void AY8910::Generator::setPeriod(int value)
{
	// Careful studies of the chip output prove that it instead counts up from
	// 0 until the counter becomes greater or equal to the period. This is an
	// important difference when the program is rapidly changing the period to
	// modulate the sound.
	// Also, note that period = 0 is the same as period = 1. This is mentioned
	// in the YM2203 data sheets. However, this does NOT apply to the Envelope
	// period. In that case, period = 0 is half as period = 1.
	period = std::max(1, value);
	count = std::min(count, period - 1);
}

inline unsigned AY8910::Generator::getNextEventTime() const
{
	assert(count < period);
	return period - count;
}

inline void AY8910::Generator::advanceFast(unsigned duration)
{
	count += narrow<int>(duration);
	assert(count < period);
}


// ToneGenerator:

AY8910::ToneGenerator::ToneGenerator()
{
	reset();
}

inline void AY8910::ToneGenerator::reset()
{
	Generator::reset();
	output = false;
}

int AY8910::ToneGenerator::getDetune(const AY8910& ay8910)
{
	int result = 0;
	if (float vibPerc = ay8910.vibratoPercent.getFloat();
	    vibPerc != 0.0f) {
		auto vibratoPeriod = int(
			NATIVE_FREQ_FLOAT /
			ay8910.vibratoFrequency.getFloat());
		vibratoCount += period;
		vibratoCount %= vibratoPeriod;
		result += narrow_cast<int>(
			sinf((float(2 * Math::pi) * narrow_cast<float>(vibratoCount)) / narrow_cast<float>(vibratoPeriod))
			* vibPerc * 0.01f * narrow_cast<float>(period));
	}
	if (float detunePerc = ay8910.detunePercent.getFloat();
	    detunePerc != 0.0f) {
		float detunePeriod = NATIVE_FREQ_FLOAT /
			ay8910.detuneFrequency.getFloat();
		detuneCount += period;
		float noiseIdx = narrow_cast<float>(detuneCount) / detunePeriod;
		float detuneNoise = noiseValue(       noiseIdx)
		                  + noiseValue(2.0f * noiseIdx) * 0.5f;
		result += narrow_cast<int>(detuneNoise * detunePerc * 0.01f * narrow_cast<float>(period));
	}
	return std::min(result, period - 1);
}

inline void AY8910::ToneGenerator::advance(unsigned duration)
{
	assert(count < period);
	count += narrow<int>(duration);
	if (count >= period) {
		// Calculate number of output transitions.
		int cycles = count / period;
		count -= period * cycles; // equivalent to count %= period;
		output ^= cycles & 1;
	}
}

inline void AY8910::ToneGenerator::doNextEvent(const AY8910& ay8910)
{
	if (ay8910.doDetune) [[unlikely]] {
		count = getDetune(ay8910);
	} else {
		count = 0;
	}
	output ^= 1;
}


// NoiseGenerator:

AY8910::NoiseGenerator::NoiseGenerator()
{
	reset();
}

inline void AY8910::NoiseGenerator::reset()
{
	Generator::reset();
	random = 1;
}

inline void AY8910::NoiseGenerator::doNextEvent()
{
	count = 0;

	// The Random Number Generator of the 8910 is a 17-bit shift register.
	// The input to the shift register is bit0 XOR bit3 (bit0 is the
	// output). Verified on real AY8910 and YM2149 chips.
	//
	// Fibonacci configuration:
	//   random ^= ((random & 1) ^ ((random >> 3) & 1)) << 17;
	//   random >>= 1;
	// Galois configuration:
	//   if (random & 1) random ^= 0x24000;
	//   random >>= 1;
	// or alternatively:
	random = (random >> 1) ^ ((random & 1) << 13) ^ ((random & 1) << 16);
}

inline void AY8910::NoiseGenerator::advance(unsigned duration)
{
	assert(count < period);
	count += narrow<int>(duration);
	int cycles = count / period;
	count -= cycles * period; // equivalent to count %= period

	// The following loops advance the random state N steps at once. The
	// values for N (4585, 275, 68, 8, 1) are chosen so that:
	// - The formulas are relatively simple.
	// - The ratio between the step sizes is roughly the same.
	for (/**/; cycles >= 4585; cycles -= 4585) {
		random = ((random & 0x1f) << 12)
		       ^ ((random & 0x1f) <<  9)
		       ^   random
		       ^ ( random         >>  5);
	}
	for (/**/; cycles >= 275; cycles -= 275) {
		random = ((random & 0x03f) << 11)
		       ^ ((random & 0x1c0) <<  8)
		       ^ ((random & 0x1ff) <<  5)
		       ^   random
		       ^ ( random          >>  6)
		       ^ ( random          >>  9);
	}
	for (/**/; cycles >= 68; cycles -= 68) {
		random = ((random & 0xfff) <<  5)
		       ^ ((random & 0xfff) <<  2)
		       ^   random
		       ^ ( random          >> 12);
	}
	for (/**/; cycles >= 8; cycles -= 8) {
		random = ((random & 0xff) << 9)
		       ^ ((random & 0xff) << 6)
		       ^ ( random         >> 8);
	}
	for (/**/; cycles >= 1; cycles -= 1) {
		random = ((random & 1) << 16)
		       ^ ((random & 1) << 13)
		       ^ ( random      >>  1);
	}
}


// Amplitude:

static bool checkAY8910(const DeviceConfig& config)
{
	auto type = config.getChildData("type", "ay8910");
	StringOp::casecmp cmp;
	if (cmp(type, "ay8910")) {
		return true;
	} else if (cmp(type, "ym2149")) {
		return false;
	}
	throw FatalError("Unknown PSG type: ", type);
}

AY8910::Amplitude::Amplitude(const DeviceConfig& config)
	: isAY8910(checkAY8910(config))
	, envVolTable(isAY8910 ? AY8910EnvelopeTab : YM2149EnvelopeTab)
{
	vol[0] = vol[1] = vol[2] = 0.0f;
	envChan[0] = false;
	envChan[1] = false;
	envChan[2] = false;

	if (false) {
		std::cout << "YM2149Envelope:";
		for (const auto& e : YM2149EnvelopeTab) {
			std::cout << ' ' << std::hexfloat << e;
		}
		std::cout << "\nAY8910Envelope:";
		for (const auto& e : AY8910EnvelopeTab) {
			std::cout << ' ' << std::hexfloat << e;
		}
		std::cout << "\nvolume:";
		for (const auto& e : volumeTab) {
			std::cout << ' ' << std::hexfloat << e;
		}
		std::cout << '\n';
	}
}

inline float AY8910::Amplitude::getVolume(unsigned chan) const
{
	assert(!followsEnvelope(chan));
	return vol[chan];
}

inline void AY8910::Amplitude::setChannelVolume(unsigned chan, unsigned value)
{
	envChan[chan] = (value & 0x10) != 0;
	vol[chan] = volumeTab[value & 0x0F];
}

inline bool AY8910::Amplitude::followsEnvelope(unsigned chan) const
{
	return envChan[chan];
}


// Envelope:

// AY8910 and YM2149 behave different here:
//  YM2149 envelope goes twice as fast and has twice as many levels. Here
//  we implement the YM2149 behaviour, but to get the AY8910 behaviour we
//  repeat every level twice in the envVolTable

inline AY8910::Envelope::Envelope(std::span<const float, 32> envVolTable_)
	: envVolTable(envVolTable_)
{
}

inline void AY8910::Envelope::reset()
{
	count = 0;
}

inline void AY8910::Envelope::setPeriod(int value)
{
	// twice as fast as AY8910
	//  see also Generator::setPeriod()
	period = std::max(1, 2 * value);
	count = std::min(count, period - 1);
}

inline float AY8910::Envelope::getVolume() const
{
	return envVolTable[step ^ attack];
}

inline void AY8910::Envelope::setShape(unsigned shape)
{
	// do 32 steps for both AY8910 and YM2149
	/*
	envelope shapes:
		C AtAlH
		0 0 x x  \___
		0 1 x x  /___
		1 0 0 0  \\\\
		1 0 0 1  \___
		1 0 1 0  \/\/
		1 0 1 1  \
		1 1 0 0  ////
		1 1 0 1  /
		1 1 1 0  /\/\
		1 1 1 1  /___
	*/
	attack = (shape & 0x04) ? 0x1F : 0x00;
	if ((shape & 0x08) == 0) {
		// If Continue = 0, map the shape to the equivalent one
		// which has Continue = 1.
		hold = true;
		alternate = attack != 0;
	} else {
		hold = (shape & 0x01) != 0;
		alternate = (shape & 0x02) != 0;
	}
	count = 0;
	step = 0x1F;
	holding = false;
}

inline bool AY8910::Envelope::isChanging() const
{
	return !holding;
}

inline void AY8910::Envelope::doSteps(int steps)
{
	// For best performance callers should check upfront whether
	//    isChanging() == true
	// Though we can't assert on it because the condition might change
	// in the inner loop(s) of generateChannels().
	//assert(!holding);

	if (holding) return;
	step -= steps;

	// Check current envelope position.
	if (step < 0) {
		if (hold) {
			if (alternate) attack ^= 0x1F;
			holding = true;
			step = 0;
		} else {
			// If step has looped an odd number of times
			// (usually 1), invert the output.
			if (alternate && (step & 0x10)) {
				attack ^= 0x1F;
			}
			step &= 0x1F;
		}
	}
}

inline void AY8910::Envelope::advance(unsigned duration)
{
	assert(count < period);
	count += narrow<int>(duration * 2);
	if (count >= period) {
		int steps = count / period;
		count -= steps * period; // equivalent to count %= period;
		doSteps(steps);
	}
}

inline void AY8910::Envelope::doNextEvent()
{
	count = 0;
	doSteps(period == 1 ? 2 : 1);
}

inline unsigned AY8910::Envelope::getNextEventTime() const
{
	assert(count < period);
	return (period - count + 1) / 2;
}

inline void AY8910::Envelope::advanceFast(unsigned duration)
{
	count += narrow<int>(2 * duration);
	assert(count < period);
}



// AY8910 main class:

AY8910::AY8910(const std::string& name_, AY8910Periphery& periphery_,
               const DeviceConfig& config, EmuTime::param time)
	: ResampledSoundDevice(config.getMotherBoard(), name_, "PSG", 3, NATIVE_FREQ_INT, false)
	, periphery(periphery_)
	, debuggable(config.getMotherBoard(), getName())
	, vibratoPercent(
		config.getCommandController(), tmpStrCat(getName(), "_vibrato_percent"),
		"controls strength of vibrato effect", 0.0, 0.0, 10.0)
	, vibratoFrequency(
		config.getCommandController(), tmpStrCat(getName(), "_vibrato_frequency"),
		"frequency of vibrato effect in Hertz", 5, 1.0, 10.0)
	, detunePercent(
		config.getCommandController(), tmpStrCat(getName(), "_detune_percent"),
		"controls strength of detune effect", 0.0, 0.0, 10.0)
	, detuneFrequency(
		config.getCommandController(), tmpStrCat(getName(), "_detune_frequency"),
		"frequency of detune effect in Hertz", 5.0, 1.0, 100.0)
	, directionsCallback(
		config.getGlobalSettings().getInvalidPsgDirectionsSetting())
	, amplitude(config)
	, envelope(amplitude.getEnvVolTable())
	, isAY8910(checkAY8910(config))
	, ignorePortDirections(config.getChildDataAsBool("ignorePortDirections", true))
{
	update(vibratoPercent);

	// make valgrind happy
	ranges::fill(regs, 0);

	reset(time);
	registerSound(config);

	// only attach once all initialization is successful
	vibratoPercent.attach(*this);
	detunePercent .attach(*this);
}

AY8910::~AY8910()
{
	vibratoPercent.detach(*this);
	detunePercent .detach(*this);

	unregisterSound();
}

void AY8910::reset(EmuTime::param time)
{
	// Reset generators and envelope.
	for (auto& t : tone) t.reset();
	noise.reset();
	envelope.reset();
	// Reset registers and values derived from them.
	for (auto reg : xrange(16)) {
		wrtReg(reg, 0, time);
	}
}


uint8_t AY8910::readRegister(unsigned reg, EmuTime::param time)
{
	if (reg >= 16) return 255;
	switch (reg) {
	case AY_PORTA:
		if (!(regs[AY_ENABLE] & PORT_A_DIRECTION)) { // input
			regs[reg] = periphery.readA(time);
		}
		break;
	case AY_PORTB:
		if (!(regs[AY_ENABLE] & PORT_B_DIRECTION)) { // input
			regs[reg] = periphery.readB(time);
		}
		break;
	}

	// TODO some AY8910 models have 1F as mask for registers 1, 3, 5
	static constexpr std::array<uint8_t, 16> regMask = {
		0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0x1f, 0xff,
		0x1f, 0x1f ,0x1f, 0xff, 0xff, 0x0f, 0xff, 0xff
	};
	return isAY8910 ? regs[reg] & regMask[reg]
	                : regs[reg];
}

uint8_t AY8910::peekRegister(unsigned reg, EmuTime::param time) const
{
	if (reg >= 16) return 255;
	switch (reg) {
	case AY_PORTA:
		if (!(regs[AY_ENABLE] & PORT_A_DIRECTION)) { // input
			return periphery.readA(time);
		}
		break;
	case AY_PORTB:
		if (!(regs[AY_ENABLE] & PORT_B_DIRECTION)) { // input
			return periphery.readB(time);
		}
		break;
	}
	return regs[reg];
}


void AY8910::writeRegister(unsigned reg, uint8_t value, EmuTime::param time)
{
	if (reg >= 16) return;
	if ((reg < AY_PORTA) && (reg == AY_ESHAPE || regs[reg] != value)) {
		// Update the output buffer before changing the register.
		updateStream(time);
	}
	wrtReg(reg, value, time);
}
void AY8910::wrtReg(unsigned reg, uint8_t value, EmuTime::param time)
{
	// Warn/force port directions
	if (reg == AY_ENABLE) {
		if (value & PORT_A_DIRECTION) {
			directionsCallback.execute();
		}
		if (ignorePortDirections)
		{
			// portA -> input
			// portB -> output
			value = (value & ~PORT_A_DIRECTION) | PORT_B_DIRECTION;
		}
	}

	// Note: unused bits are stored as well; they can be read back.
	uint8_t diff = regs[reg] ^ value;
	regs[reg] = value;

	switch (reg) {
	case AY_AFINE:
	case AY_ACOARSE:
	case AY_BFINE:
	case AY_BCOARSE:
	case AY_CFINE:
	case AY_CCOARSE:
		tone[reg / 2].setPeriod(regs[reg & ~1] + 256 * (regs[reg | 1] & 0x0F));
		break;
	case AY_NOISEPER:
		// Half the frequency of tone generation.
		//
		// Verified on turboR GT: value=0 and value=1 sound the same.
		//
		// Likely in real AY8910 this is implemented by driving the
		// noise generator at halve the frequency instead of
		// multiplying the value by 2 (hence the correction for value=0
		// here). But the effect is the same(?).
		noise.setPeriod(2 * std::max(1, value & 0x1F));
		break;
	case AY_AVOL:
	case AY_BVOL:
	case AY_CVOL:
		amplitude.setChannelVolume(reg - AY_AVOL, value);
		break;
	case AY_EFINE:
	case AY_ECOARSE:
		// also half the frequency of tone generation, but handled
		// inside Envelope::setPeriod()
		envelope.setPeriod(regs[AY_EFINE] + 256 * regs[AY_ECOARSE]);
		break;
	case AY_ESHAPE:
		envelope.setShape(value);
		break;
	case AY_ENABLE:
		if (diff & PORT_A_DIRECTION) {
			// port A changed
			if (value & PORT_A_DIRECTION) {
				// from input to output
				periphery.writeA(regs[AY_PORTA], time);
			} else {
				// from output to input
				periphery.writeA(0xff, time);
			}
		}
		if (diff & PORT_B_DIRECTION) {
			// port B changed
			if (value & PORT_B_DIRECTION) {
				// from input to output
				periphery.writeB(regs[AY_PORTB], time);
			} else {
				// from output to input
				periphery.writeB(0xff, time);
			}
		}
		break;
	case AY_PORTA:
		if (regs[AY_ENABLE] & PORT_A_DIRECTION) { // output
			periphery.writeA(value, time);
		}
		break;
	case AY_PORTB:
		if (regs[AY_ENABLE] & PORT_B_DIRECTION) { // output
			periphery.writeB(value, time);
		}
		break;
	}
}

[[nodiscard]] static inline float calc(bool b, float f)
{
	// Calculates:
	//   return b ? f : 0.0f;
	// Though this generates branchless code, might be faster
	return narrow<float>(b) * f;
}
[[nodiscard]] static inline float calc(bool b1, bool b2, float f)
{
	return narrow<float>(b1 && b2) * f;
}

void AY8910::generateChannels(std::span<float*> bufs, unsigned num)
{
	// Disable channels with volume 0: since the sample value doesn't matter,
	// we can use the fastest path.
	unsigned chanEnable = regs[AY_ENABLE];
	for (auto chan : xrange(3)) {
		if ((!amplitude.followsEnvelope(chan) &&
		     (amplitude.getVolume(chan) == 0.0f)) ||
		    (amplitude.followsEnvelope(chan) &&
		     !envelope.isChanging() &&
		     (envelope.getVolume() == 0.0f))) {
			bufs[chan] = nullptr;
			tone[chan].advance(num);
			chanEnable |= 0x09 << chan;
		}
	}
	// Noise disabled on all channels?
	if ((chanEnable & 0x38) == 0x38) {
		noise.advance(num);
	}

	// Calculate samples.
	// The 8910 has three outputs, each output is the mix of one of the
	// three tone generators and of the (single) noise generator. The two
	// are mixed BEFORE going into the DAC. The formula to mix each channel
	// is:
	//   (ToneOn | ToneDisable) & (NoiseOn | NoiseDisable),
	//   where ToneOn and NoiseOn are the current generator state
	//   and ToneDisable and NoiseDisable come from the enable reg.
	// Note that this means that if both tone and noise are disabled, the
	// output is 1, not 0, and can be modulated by changing the volume.
	bool envelopeUpdated = false;
	Envelope initialEnvelope = envelope;
	NoiseGenerator initialNoise = noise;
	for (unsigned chan = 0; chan < 3; ++chan, chanEnable >>= 1) {
		auto* buf = bufs[chan];
		if (!buf) continue;
		ToneGenerator& t = tone[chan];
		if (envelope.isChanging() && amplitude.followsEnvelope(chan)) {
			envelopeUpdated = true;
			envelope = initialEnvelope;
			if ((chanEnable & 0x09) == 0x08) {
				// no noise, square wave: alternating between 0 and 1.
				auto val = calc(t.getOutput(), envelope.getVolume());
				unsigned remaining = num;
				unsigned nextE = envelope.getNextEventTime();
				unsigned nextT = t.getNextEventTime();
				while ((nextT <= remaining) || (nextE <= remaining)) {
					if (nextT < nextE) {
						addFill(buf, val, nextT);
						remaining -= nextT;
						nextE -= nextT;
						envelope.advanceFast(nextT);
						t.doNextEvent(*this);
						nextT = t.getNextEventTime();
					} else if (nextE < nextT) {
						addFill(buf, val, nextE);
						remaining -= nextE;
						nextT -= nextE;
						t.advanceFast(nextE);
						envelope.doNextEvent();
						nextE = envelope.getNextEventTime();
					} else {
						assert(nextT == nextE);
						addFill(buf, val, nextT);
						remaining -= nextT;
						t.doNextEvent(*this);
						nextT = t.getNextEventTime();
						envelope.doNextEvent();
						nextE = envelope.getNextEventTime();
					}
					val = calc(t.getOutput(), envelope.getVolume());
				}
				if (remaining) {
					// last interval (without events)
					addFill(buf, val, remaining);
					t.advanceFast(remaining);
					envelope.advanceFast(remaining);
				}

			} else if ((chanEnable & 0x09) == 0x09) {
				// no noise, channel disabled: always 1.
				auto val = envelope.getVolume();
				unsigned remaining = num;
				unsigned next = envelope.getNextEventTime();
				while (next <= remaining) {
					addFill(buf, val, next);
					remaining -= next;
					envelope.doNextEvent();
					val = envelope.getVolume();
					next = envelope.getNextEventTime();
				}
				if (remaining) {
					// last interval (without events)
					addFill(buf, val, remaining);
					envelope.advanceFast(remaining);
				}
				t.advance(num);

			} else if ((chanEnable & 0x09) == 0x00) {
				// noise enabled, tone enabled
				noise = initialNoise;
				auto val = calc(noise.getOutput(), t.getOutput(), envelope.getVolume());
				unsigned remaining = num;
				unsigned nextT = t.getNextEventTime();
				unsigned nextN = noise.getNextEventTime();
				unsigned nextE = envelope.getNextEventTime();
				unsigned next = std::min({nextT, nextN, nextE});
				while (next <= remaining) {
					addFill(buf, val, next);
					remaining -= next;
					nextT -= next;
					nextN -= next;
					nextE -= next;
					if (nextT) {
						t.advanceFast(next);
					} else {
						t.doNextEvent(*this);
						nextT = t.getNextEventTime();
					}
					if (nextN) {
						noise.advanceFast(next);
					} else {
						noise.doNextEvent();
						nextN = noise.getNextEventTime();
					}
					if (nextE) {
						envelope.advanceFast(next);
					} else {
						envelope.doNextEvent();
						nextE = envelope.getNextEventTime();
					}
					next = std::min({nextT, nextN, nextE});
					val = calc(noise.getOutput(), t.getOutput(), envelope.getVolume());
				}
				if (remaining) {
					// last interval (without events)
					addFill(buf, val, remaining);
					t.advanceFast(remaining);
					noise.advanceFast(remaining);
					envelope.advanceFast(remaining);
				}

			} else {
				// noise enabled, tone disabled
				noise = initialNoise;
				auto val = calc(noise.getOutput(), envelope.getVolume());
				unsigned remaining = num;
				unsigned nextE = envelope.getNextEventTime();
				unsigned nextN = noise.getNextEventTime();
				while ((nextN <= remaining) || (nextE <= remaining)) {
					if (nextN < nextE) {
						addFill(buf, val, nextN);
						remaining -= nextN;
						nextE -= nextN;
						envelope.advanceFast(nextN);
						noise.doNextEvent();
						nextN = noise.getNextEventTime();
					} else if (nextE < nextN) {
						addFill(buf, val, nextE);
						remaining -= nextE;
						nextN -= nextE;
						noise.advanceFast(nextE);
						envelope.doNextEvent();
						nextE = envelope.getNextEventTime();
					} else {
						assert(nextN == nextE);
						addFill(buf, val, nextN);
						remaining -= nextN;
						noise.doNextEvent();
						nextN = noise.getNextEventTime();
						envelope.doNextEvent();
						nextE = envelope.getNextEventTime();
					}
					val = calc(noise.getOutput(), envelope.getVolume());
				}
				if (remaining) {
					// last interval (without events)
					addFill(buf, val, remaining);
					noise.advanceFast(remaining);
					envelope.advanceFast(remaining);
				}
				t.advance(num);
			}
		} else {
			// no (changing) envelope on this channel
			auto volume = amplitude.followsEnvelope(chan)
			            ? envelope.getVolume()
			            : amplitude.getVolume(chan);
			if ((chanEnable & 0x09) == 0x08) {
				// no noise, square wave: alternating between 0 and 1.
				auto val = calc(t.getOutput(), volume);
				unsigned remaining = num;
				unsigned next = t.getNextEventTime();
				while (next <= remaining) {
					addFill(buf, val, next);
					val = volume - val;
					remaining -= next;
					t.doNextEvent(*this);
					next = t.getNextEventTime();
				}
				if (remaining) {
					// last interval (without events)
					addFill(buf, val, remaining);
					t.advanceFast(remaining);
				}

			} else if ((chanEnable & 0x09) == 0x09) {
				// no noise, channel disabled: always 1.
				addFill(buf, volume, num);
				t.advance(num);

			} else if ((chanEnable & 0x09) == 0x00) {
				// noise enabled, tone enabled
				noise = initialNoise;
				auto val1 = calc(t.getOutput(), volume);
				auto val2 = calc(noise.getOutput(), val1);
				unsigned remaining = num;
				unsigned nextN = noise.getNextEventTime();
				unsigned nextT = t.getNextEventTime();
				while ((nextN <= remaining) || (nextT <= remaining)) {
					if (nextT < nextN) {
						addFill(buf, val2, nextT);
						remaining -= nextT;
						nextN -= nextT;
						noise.advanceFast(nextT);
						t.doNextEvent(*this);
						nextT = t.getNextEventTime();
						val1 = volume - val1;
						val2 = calc(noise.getOutput(), val1);
					} else if (nextN < nextT) {
						addFill(buf, val2, nextN);
						remaining -= nextN;
						nextT -= nextN;
						t.advanceFast(nextN);
						noise.doNextEvent();
						nextN = noise.getNextEventTime();
						val2 = calc(noise.getOutput(), val1);
					} else {
						assert(nextT == nextN);
						addFill(buf, val2, nextT);
						remaining -= nextT;
						t.doNextEvent(*this);
						nextT = t.getNextEventTime();
						noise.doNextEvent();
						nextN = noise.getNextEventTime();
						val1 = volume - val1;
						val2 = calc(noise.getOutput(), val1);
					}
				}
				if (remaining) {
					// last interval (without events)
					addFill(buf, val2, remaining);
					t.advanceFast(remaining);
					noise.advanceFast(remaining);
				}

			} else {
				// noise enabled, tone disabled
				noise = initialNoise;
				unsigned remaining = num;
				auto val = calc(noise.getOutput(), volume);
				unsigned next = noise.getNextEventTime();
				while (next <= remaining) {
					addFill(buf, val, next);
					remaining -= next;
					noise.doNextEvent();
					val = calc(noise.getOutput(), volume);
					next = noise.getNextEventTime();
				}
				if (remaining) {
					// last interval (without events)
					addFill(buf, val, remaining);
					noise.advanceFast(remaining);
				}
				t.advance(num);
			}
		}
	}

	// Envelope not yet updated?
	if (envelope.isChanging() && !envelopeUpdated) {
		envelope.advance(num);
	}
}

float AY8910::getAmplificationFactorImpl() const
{
	return 1.0f;
}

void AY8910::update(const Setting& setting) noexcept
{
	if (&setting == one_of(&vibratoPercent, &detunePercent)) {
		doDetune = (vibratoPercent.getFloat() != 0.0f) ||
			   (detunePercent .getFloat() != 0.0f);
	} else {
		ResampledSoundDevice::update(setting);
	}
}


// Debuggable

AY8910::Debuggable::Debuggable(MSXMotherBoard& motherBoard_, const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs", "PSG", 0x10)
{
}

uint8_t AY8910::Debuggable::read(unsigned address, EmuTime::param time)
{
	auto& ay8910 = OUTER(AY8910, debuggable);
	return ay8910.readRegister(address, time);
}

void AY8910::Debuggable::write(unsigned address, uint8_t value, EmuTime::param time)
{
	auto& ay8910 = OUTER(AY8910, debuggable);
	return ay8910.writeRegister(address, value, time);
}


template<typename Archive>
void AY8910::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("toneGenerators", tone,
	             "noiseGenerator", noise,
	             "envelope",       envelope,
	             "registers",      regs);

	// amplitude
	if constexpr (Archive::IS_LOADER) {
		for (auto i : xrange(3)) {
			amplitude.setChannelVolume(i, regs[i + AY_AVOL]);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(AY8910);

// version 1: initial version
// version 2: removed 'output' member variable
template<typename Archive>
void AY8910::Generator::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("period", period,
	             "count", count);
}
INSTANTIATE_SERIALIZE_METHODS(AY8910::Generator);

// version 1: initial version
// version 2: moved 'output' variable from base class to here
template<typename Archive>
void AY8910::ToneGenerator::serialize(Archive& ar, unsigned version)
{
	ar.template serializeInlinedBase<Generator>(*this, version);
	ar.serialize("vibratoCount", vibratoCount,
	             "detuneCount", detuneCount);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("output", output);
	} else {
		// don't bother trying to restore this from the old location:
		// it doesn't influence any MSX-observable state, and the
		// difference in generated sound will likely be inaudible
	}
}
INSTANTIATE_SERIALIZE_METHODS(AY8910::ToneGenerator);

// version 1: initial version
// version 2: removed 'output' variable from base class, not stored here but
//            instead it's calculated from 'random' when needed
template<typename Archive>
void AY8910::NoiseGenerator::serialize(Archive& ar, unsigned version)
{
	ar.template serializeInlinedBase<Generator>(*this, version);
	ar.serialize("random", random);
}
INSTANTIATE_SERIALIZE_METHODS(AY8910::NoiseGenerator);

template<typename Archive>
void AY8910::Envelope::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("period",    period,
	             "count",     count,
	             "step",      step,
	             "attack",    attack,
	             "hold",      hold,
	             "alternate", alternate,
	             "holding",   holding);
}
INSTANTIATE_SERIALIZE_METHODS(AY8910::Envelope);

} // namespace openmsx
