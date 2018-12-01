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
#include "likely.hh"
#include "outer.hh"
#include "random.hh"
#include <cassert>
#include <cmath>
#include <cstring>

using std::string;

namespace openmsx {

// The step clock for the tone and noise generators is the chip clock
// divided by 8; for the envelope generator of the AY-3-8910, it is half
// that much (clock/16).
static const float NATIVE_FREQ_FLOAT = (3579545.0f / 2) / 8;
static const int NATIVE_FREQ_INT = lrintf(NATIVE_FREQ_FLOAT);

static const int PORT_A_DIRECTION = 0x40;
static const int PORT_B_DIRECTION = 0x80;

enum Register {
	AY_AFINE = 0, AY_ACOARSE = 1, AY_BFINE = 2, AY_BCOARSE = 3,
	AY_CFINE = 4, AY_CCOARSE = 5, AY_NOISEPER = 6, AY_ENABLE = 7,
	AY_AVOL = 8, AY_BVOL = 9, AY_CVOL = 10, AY_EFINE = 11,
	AY_ECOARSE = 12, AY_ESHAPE = 13, AY_PORTA = 14, AY_PORTB = 15
};

// Perlin noise

static float noiseTab[256 + 3];

static void initDetune()
{
	auto& generator = global_urng(); // fast (non-cryptographic) random numbers
	std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

	for (int i = 0; i < 256; ++i) {
		noiseTab[i] = distribution(generator);
	}
	noiseTab[256] = noiseTab[0];
	noiseTab[257] = noiseTab[1];
	noiseTab[258] = noiseTab[2];
}
static float noiseValue(float x)
{
	// cubic hermite spline interpolation
	assert(0.0f <= x);
	int xi = int(x);
	float xf = x - xi;
	xi &= 255;
	float n0 = noiseTab[xi + 0];
	float n1 = noiseTab[xi + 1];
	float n2 = noiseTab[xi + 2];
	float n3 = noiseTab[xi + 3];
	float a = n3 - n2 + n1 - n0;
	float b = n0 - n1 - a;
	float c = n2 - n0;
	float d = n1;
	return ((a * xf + b) * xf + c) * xf + d;
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
	count += duration;
	assert(count < period);
}


// ToneGenerator:

AY8910::ToneGenerator::ToneGenerator()
	: vibratoCount(0), detuneCount(0)
{
	reset();
}

inline void AY8910::ToneGenerator::reset()
{
	Generator::reset();
	output = false;
}

int AY8910::ToneGenerator::getDetune(AY8910& ay8910)
{
	int result = 0;
	float vibPerc = ay8910.vibratoPercent.getDouble();
	if (vibPerc != 0.0f) {
		int vibratoPeriod = int(
			NATIVE_FREQ_FLOAT /
			float(ay8910.vibratoFrequency.getDouble()));
		vibratoCount += period;
		vibratoCount %= vibratoPeriod;
		result += int(
			sinf((float(2 * M_PI) * vibratoCount) / vibratoPeriod)
			* vibPerc * 0.01f * period);
	}
	float detunePerc = ay8910.detunePercent.getDouble();
	if (detunePerc != 0.0f) {
		float detunePeriod = NATIVE_FREQ_FLOAT /
			float(ay8910.detuneFrequency.getDouble());
		detuneCount += period;
		float noiseIdx = detuneCount / detunePeriod;
		float detuneNoise = noiseValue(       noiseIdx)
		                  + noiseValue(2.0f * noiseIdx) / 2.0f;
		result += int(detuneNoise * detunePerc * 0.01f * period);
	}
	return std::min(result, period - 1);
}

inline void AY8910::ToneGenerator::advance(int duration)
{
	assert(count < period);
	count += duration;
	if (count >= period) {
		// Calculate number of output transitions.
		int cycles = count / period;
		count -= period * cycles; // equivalent to count %= period;
		output ^= cycles & 1;
	}
}

inline void AY8910::ToneGenerator::doNextEvent(AY8910& ay8910)
{
	if (unlikely(ay8910.doDetune)) {
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
	// Fibonacci configuartion:
	//   random ^= ((random & 1) ^ ((random >> 3) & 1)) << 17;
	//   random >>= 1;
	// Galois configuration:
	//   if (random & 1) random ^= 0x24000;
	//   random >>= 1;
	// or alternatively:
	random = (random >> 1) ^ ((random & 1) << 13) ^ ((random & 1) << 16);
}

inline void AY8910::NoiseGenerator::advance(int duration)
{
	assert(count < period);
	count += duration;
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
	string type = StringOp::toLower(config.getChildData("type", "ay8910"));
	if (type == "ay8910") {
		return true;
	} else if (type == "ym2149") {
		return false;
	} else {
		throw FatalError("Unknown PSG type: ", type);
	}
}

AY8910::Amplitude::Amplitude(const DeviceConfig& config)
	: isAY8910(checkAY8910(config))
{
	vol[0] = vol[1] = vol[2] = 0;
	envChan[0] = false;
	envChan[1] = false;
	envChan[2] = false;
	setMasterVolume(32768);
}

const unsigned* AY8910::Amplitude::getEnvVolTable() const
{
	return envVolTable;
}

inline unsigned AY8910::Amplitude::getVolume(unsigned chan) const
{
	assert(!followsEnvelope(chan));
	return vol[chan];
}

inline void AY8910::Amplitude::setChannelVolume(unsigned chan, unsigned value)
{
	envChan[chan] = (value & 0x10) != 0;
	vol[chan] = volTable[value & 0x0F];
}

inline void AY8910::Amplitude::setMasterVolume(int volume)
{
	// Calculate the volume->voltage conversion table.
	// The AY-3-8910 has 16 levels, in a logarithmic scale (3dB per step).
	// YM2149 has 32 levels, the 16 extra levels are only used for envelope
	// volumes

	float out = volume; // avoid clipping
	float factor = powf(0.5f, 0.25f); // 1/sqrt(sqrt(2)) ~= 1/(1.5dB)
	for (int i = 31; i > 0; --i) {
		envVolTable[i] = lrintf(out); // round to nearest;
		out *= factor;
	}
	envVolTable[0] = 0;
	volTable[0] = 0;
	for (int i = 1; i < 16; ++i) {
		volTable[i] = envVolTable[2 * i + 1];
	}
	if (isAY8910) {
		// only 16 envelope steps, duplicate every step
		envVolTable[1] = 0;
		for (int i = 2; i < 32; i += 2) {
			envVolTable[i] = envVolTable[i + 1];
		}
	}
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

inline AY8910::Envelope::Envelope(const unsigned* envVolTable_)
{
	envVolTable = envVolTable_;
	period = 1;
	count  = 0;
	step   = 0;
	attack = 0;
	hold      = false;
	alternate = false;
	holding   = false;
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

inline unsigned AY8910::Envelope::getVolume() const
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

inline void AY8910::Envelope::advance(int duration)
{
	assert(count < period);
	count += duration * 2;
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
	count += 2 * duration;
	assert(count < period);
}



// AY8910 main class:

AY8910::AY8910(const std::string& name_, AY8910Periphery& periphery_,
               const DeviceConfig& config, EmuTime::param time)
	: ResampledSoundDevice(config.getMotherBoard(), name_, "PSG", 3)
	, periphery(periphery_)
	, debuggable(config.getMotherBoard(), getName())
	, vibratoPercent(
		config.getCommandController(), getName() + "_vibrato_percent",
		"controls strength of vibrato effect", 0.0, 0.0, 10.0)
	, vibratoFrequency(
		config.getCommandController(), getName() + "_vibrato_frequency",
		"frequency of vibrato effect in Hertz", 5, 1.0, 10.0)
	, detunePercent(
		config.getCommandController(), getName() + "_detune_percent",
		"controls strength of detune effect", 0.0, 0.0, 10.0)
	, detuneFrequency(
		config.getCommandController(), getName() + "_detune_frequency",
		"frequency of detune effect in Hertz", 5.0, 1.0, 100.0)
	, directionsCallback(
		config.getGlobalSettings().getInvalidPsgDirectionsSetting())
	, amplitude(config)
	, envelope(amplitude.getEnvVolTable())
	, isAY8910(checkAY8910(config))
{
	// (lazily) initialize detune stuff
	detuneInitialized = false;
	update(vibratoPercent);

	// make valgrind happy
	memset(regs, 0, sizeof(regs));

	setInputRate(NATIVE_FREQ_INT);

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
	for (unsigned reg = 0; reg <= 15; ++reg) {
		wrtReg(reg, 0, time);
	}
}


byte AY8910::readRegister(unsigned reg, EmuTime::param time)
{
	assert(reg <= 15);
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
	static const byte regMask[16] = {
		0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0x1f, 0xff,
		0x1f, 0x1f ,0x1f, 0xff, 0xff, 0x0f, 0xff, 0xff
	};
	return isAY8910 ? regs[reg] & regMask[reg]
	                : regs[reg];
}

byte AY8910::peekRegister(unsigned reg, EmuTime::param time) const
{
	assert(reg <= 15);
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


void AY8910::writeRegister(unsigned reg, byte value, EmuTime::param time)
{
	assert(reg <= 15);
	if ((reg < AY_PORTA) && (reg == AY_ESHAPE || regs[reg] != value)) {
		// Update the output buffer before changing the register.
		updateStream(time);
	}
	wrtReg(reg, value, time);
}
void AY8910::wrtReg(unsigned reg, byte value, EmuTime::param time)
{
	// Warn/force port directions
	if (reg == AY_ENABLE) {
		if (value & PORT_A_DIRECTION) {
			directionsCallback.execute();
		}
		// portA -> input
		// portB -> output
		value = (value & ~PORT_A_DIRECTION) | PORT_B_DIRECTION;
	}

	// Note: unused bits are stored as well; they can be read back.
	byte oldValue = regs[reg];
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
		// half the frequency of tone generation
		noise.setPeriod(2 * (value & 0x1F));
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
		if ((value     & PORT_A_DIRECTION) &&
		    !(oldValue & PORT_A_DIRECTION)) {
			// Changed from input to output.
			periphery.writeA(regs[AY_PORTA], time);
		}
		if ((value     & PORT_B_DIRECTION) &&
		    !(oldValue & PORT_B_DIRECTION)) {
			// Changed from input to output.
			periphery.writeB(regs[AY_PORTB], time);
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

void AY8910::generateChannels(int** bufs, unsigned num)
{
	// Disable channels with volume 0: since the sample value doesn't matter,
	// we can use the fastest path.
	unsigned chanEnable = regs[AY_ENABLE];
	for (unsigned chan = 0; chan < 3; ++chan) {
		if ((!amplitude.followsEnvelope(chan) &&
		     (amplitude.getVolume(chan) == 0)) ||
		    (amplitude.followsEnvelope(chan) &&
		     !envelope.isChanging() &&
		     (envelope.getVolume() == 0))) {
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
		int* buf = bufs[chan];
		if (!buf) continue;
		ToneGenerator& t = tone[chan];
		if (envelope.isChanging() && amplitude.followsEnvelope(chan)) {
			envelopeUpdated = true;
			envelope = initialEnvelope;
			if ((chanEnable & 0x09) == 0x08) {
				// no noise, square wave: alternating between 0 and 1.
				unsigned val = t.getOutput() * envelope.getVolume();
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
					val = t.getOutput() * envelope.getVolume();
				}
				if (remaining) {
					// last interval (without events)
					addFill(buf, val, remaining);
					t.advanceFast(remaining);
					envelope.advanceFast(remaining);
				}

			} else if ((chanEnable & 0x09) == 0x09) {
				// no noise, channel disabled: always 1.
				unsigned val = envelope.getVolume();
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
				unsigned val = noise.getOutput() * t.getOutput() * envelope.getVolume();
				unsigned remaining = num;
				unsigned nextT = t.getNextEventTime();
				unsigned nextN = noise.getNextEventTime();
				unsigned nextE = envelope.getNextEventTime();
				unsigned next = std::min(std::min(nextT, nextN), nextE);
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
					next = std::min(std::min(nextT, nextN), nextE);
					val = noise.getOutput() * t.getOutput() * envelope.getVolume();
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
				unsigned val = noise.getOutput() * envelope.getVolume();
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
					val = noise.getOutput() * envelope.getVolume();
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
			unsigned volume = amplitude.followsEnvelope(chan)
			                ? envelope.getVolume()
			                : amplitude.getVolume(chan);
			if ((chanEnable & 0x09) == 0x08) {
				// no noise, square wave: alternating between 0 and 1.
				unsigned val = t.getOutput() * volume;
				unsigned remaining = num;
				unsigned next = t.getNextEventTime();
				while (next <= remaining) {
					addFill(buf, val, next);
					val ^= volume;
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
				unsigned val1 = t.getOutput() * volume;
				unsigned val2 = val1 * noise.getOutput();
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
						val1 ^= volume;
						val2 = val1 * noise.getOutput();
					} else if (nextN < nextT) {
						addFill(buf, val2, nextN);
						remaining -= nextN;
						nextT -= nextN;
						t.advanceFast(nextN);
						noise.doNextEvent();
						nextN = noise.getNextEventTime();
						val2 = val1 * noise.getOutput();
					} else {
						assert(nextT == nextN);
						addFill(buf, val2, nextT);
						remaining -= nextT;
						t.doNextEvent(*this);
						nextT = t.getNextEventTime();
						noise.doNextEvent();
						nextN = noise.getNextEventTime();
						val1 ^= volume;
						val2 = val1 * noise.getOutput();
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
				unsigned val = noise.getOutput() * volume;
				unsigned next = noise.getNextEventTime();
				while (next <= remaining) {
					addFill(buf, val, next);
					remaining -= next;
					noise.doNextEvent();
					val = noise.getOutput() * volume;
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

void AY8910::update(const Setting& setting)
{
	if ((&setting == &vibratoPercent) ||
	    (&setting == &detunePercent)) {
		doDetune = (vibratoPercent.getDouble() != 0) ||
			   (detunePercent .getDouble() != 0);
		if (doDetune && !detuneInitialized) {
			detuneInitialized = true;
			initDetune();
		}
	} else {
		ResampledSoundDevice::update(setting);
	}
}


// Debuggable

AY8910::Debuggable::Debuggable(MSXMotherBoard& motherBoard_, const string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs", "PSG", 0x10)
{
}

byte AY8910::Debuggable::read(unsigned address, EmuTime::param time)
{
	auto& ay8910 = OUTER(AY8910, debuggable);
	return ay8910.readRegister(address, time);
}

void AY8910::Debuggable::write(unsigned address, byte value, EmuTime::param time)
{
	auto& ay8910 = OUTER(AY8910, debuggable);
	return ay8910.writeRegister(address, value, time);
}


template<typename Archive>
void AY8910::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("toneGenerators", tone);
	ar.serialize("noiseGenerator", noise);
	ar.serialize("envelope", envelope);
	ar.serialize("registers", regs);

	// amplitude
	if (ar.isLoader()) {
		for (int i = 0; i < 3; ++i) {
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
	ar.serialize("period", period);
	ar.serialize("count", count);
}
INSTANTIATE_SERIALIZE_METHODS(AY8910::Generator);

// version 1: initial version
// version 2: moved 'output' variable from base class to here
template<typename Archive>
void AY8910::ToneGenerator::serialize(Archive& ar, unsigned version)
{
	ar.template serializeInlinedBase<Generator>(*this, version);
	ar.serialize("vibratoCount", vibratoCount);
	ar.serialize("detuneCount", detuneCount);
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
	ar.serialize("period",    period);
	ar.serialize("count",     count);
	ar.serialize("step",      step);
	ar.serialize("attack",    attack);
	ar.serialize("hold",      hold);
	ar.serialize("alternate", alternate);
	ar.serialize("holding",   holding);
}
INSTANTIATE_SERIALIZE_METHODS(AY8910::Envelope);

} // namespace openmsx
