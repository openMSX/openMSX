// $Id$

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
#include "MSXMotherBoard.hh"
#include "MSXCliComm.hh"
#include "SimpleDebuggable.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "FloatSetting.hh"
#include "likely.hh"
#include <cassert>
#include <cmath>
#include <cstring>

using std::string;

namespace openmsx {

class AY8910Debuggable : public SimpleDebuggable
{
public:
	AY8910Debuggable(MSXMotherBoard& motherBoard, AY8910& ay8910);
	virtual byte read(unsigned address, const EmuTime& time);
	virtual void write(unsigned address, byte value, const EmuTime& time);
private:
	AY8910& ay8910;
};


// The step clock for the tone and noise generators is the chip clock
// divided by 8; for the envelope generator of the AY-3-8910, it is half
// that much (clock/16).
static const double NATIVE_FREQ_DOUBLE = (3579545.0 / 2) / 8;
static const int NATIVE_FREQ_INT = int(NATIVE_FREQ_DOUBLE + 0.5);

static const int PORT_A_DIRECTION = 0x40;
static const int PORT_B_DIRECTION = 0x80;
enum Register {
	AY_AFINE = 0, AY_ACOARSE = 1, AY_BFINE = 2, AY_BCOARSE = 3,
	AY_CFINE = 4, AY_CCOARSE = 5, AY_NOISEPER = 6, AY_ENABLE = 7,
	AY_AVOL = 8, AY_BVOL = 9, AY_CVOL = 10, AY_EFINE = 11,
	AY_ECOARSE = 12, AY_ESHAPE = 13, AY_PORTA = 14, AY_PORTB = 15
};

// Perlin noise

static float n[256 + 3];

static void initDetune()
{
	for (int i = 0; i < 256; ++i) {
		n[i] = float(rand()) / (RAND_MAX / 2) - 1.0f;
	}
	n[256] = n[0];
	n[257] = n[1];
	n[258] = n[2];
}
static float noiseValue(float x)
{
	// cubic hermite spline interpolation
	assert(0.0f <= x);
	int xi = int(x);
	float xf = x - xi;
	xi &= 255;
	float n0 = n[xi + 0];
	float n1 = n[xi + 1];
	float n2 = n[xi + 2];
	float n3 = n[xi + 3];
	float a = n3 - n2 + n1 - n0;
	float b = n0 - n1 - a;
	float c = n2 - n0;
	float d = n1;
	return ((a * xf + b) * xf + c) * xf + d;
}


// Generator:

AY8910::Generator::Generator()
{
	reset(0);
}

inline void AY8910::Generator::reset(unsigned output)
{
	count = 0;
	this->output = output;
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

inline unsigned AY8910::Generator::getOutput()
{
	return output;
}

inline unsigned AY8910::Generator::getNextEventTime()
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
	: parent(NULL), vibratoCount(0), detuneCount(0)
{
}

inline void AY8910::ToneGenerator::setParent(AY8910& parent)
{
	this->parent = &parent;
}

int AY8910::ToneGenerator::getDetune()
{
	int result = 0;
	double vibPerc = parent->vibratoPercent->getValue();
	if (vibPerc != 0.0) {
		int vibratoPeriod = int(
			NATIVE_FREQ_DOUBLE
			/ parent->vibratoFrequency->getValue());
		vibratoCount += period;
		vibratoCount %= vibratoPeriod;
		result += int(
			sin((M_PI * 2 * vibratoCount) / vibratoPeriod)
			* vibPerc * 0.01 * period);
	}
	double detunePerc = parent->detunePercent->getValue();
	if (detunePerc != 0.0) {
		double detunePeriod = NATIVE_FREQ_DOUBLE /
			parent->detuneFrequency->getValue();
		detuneCount += period;
		double noiseIdx = detuneCount / detunePeriod;
		double noise = noiseValue(noiseIdx);
		noise += noiseValue(2.0 * noiseIdx) / 2.0;
		result += int(noise * detunePerc * 0.01 * period);
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

inline void AY8910::ToneGenerator::doNextEvent(bool doDetune)
{
	if (unlikely(doDetune)) {
		count = getDetune();
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
	Generator::reset(1);
	random = 1;
}

inline void AY8910::NoiseGenerator::doNextEvent()
{
	count = 0;
	// noise output changes when (bit1 ^ bit0) == 1
	output ^= ((random + 1) & 2) >> 1;

	// The Random Number Generator of the 8910 is a 17-bit shift register.
	// The input to the shift register is bit0 XOR bit2 (bit0 is the
	// output).
	// The following is a fast way to compute bit 17 = bit0^bit2.
	// Instead of doing all the logic operations, we only check bit 0,
	// relying on the fact that after two shifts of the register, what now
	// is bit 2 will become bit 0, and will invert, if necessary, bit 16,
	// which previously was bit 18.
	// Note: On Pentium 4, the "if" causes trouble in the pipeline.
	//       After all this is pseudo-random and therefore a nightmare
	//       for branch prediction.
	//       A bit more calculation without a branch is faster.
	//       Without the "if", the transformation described above still
	//       speeds up the code, because the same "random & N"
	//       subexpression appears twice (also when doing multiple cycles
	//       in one go, see "advance" method).
	//       TODO: Benchmark on other modern CPUs.
	//if (random & 1) random ^= 0x28000;
	//random >>= 1;
	random = (random >> 1) ^ ((random & 1) << 14) ^ ((random & 1) << 16);
}

inline void AY8910::NoiseGenerator::advance(int duration)
{
	assert(count < period);
	count += duration;
	int cycles = count / period;
	count -= cycles * period; // equivalent to count %= period
	// See advanceToFlip for explanation of noise algorithm.
	for (; cycles >= 4405; cycles -= 4405) {
		random ^= (random >> 10)
		       ^ ((random & 0x003FF) << 5)
		       ^ ((random & 0x003FF) << 7);
	}
	for (; cycles >= 291; cycles -= 291) {
		random ^= (random >> 6)
		       ^ ((random & 0x3F) << 9)
		       ^ ((random & 0x3F) << 11);
	}
	for (; cycles >= 15; cycles -= 15) {
		random =  (random & 0x07FFF)
		       ^  (random >> 15)
		       ^ ((random & 0x07FFF) << 2);
	}
	while (cycles--) {
		random =  (random >> 1)
		       ^ ((random & 1) << 14)
		       ^ ((random & 1) << 16);
	}
	output = random & 1;
}


// Amplitude:

AY8910::Amplitude::Amplitude(const XMLElement& config)
{
	string type = StringOp::toLower(config.getChildData("type", "ay8910"));
	if (type == "ay8910") {
		ay8910 = true;
	} else if (type == "ym2149") {
		ay8910 = false;
	} else {
		throw FatalError("Unknown PSG type: " + type);
	}

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
	envChan[chan] = value & 0x10;
	vol[chan] = volTable[value & 0x0F];
}

inline void AY8910::Amplitude::setMasterVolume(int volume)
{
	// Calculate the volume->voltage conversion table.
	// The AY-3-8910 has 16 levels, in a logarithmic scale (3dB per step).
	// YM2149 has 32 levels, the 16 extra levels are only used for envelope
	// volumes

	double out = volume; // avoid clipping
	double factor = pow(0.5, 0.25); // 1/sqrt(sqrt(2)) ~= 1/(1.5dB)
	for (int i = 31; i > 0; --i) {
		envVolTable[i] = unsigned(out + 0.5); // round to nearest;
		out *= factor;
	}
	envVolTable[0] = 0;
	volTable[0] = 0;
	for (int i = 1; i < 16; ++i) {
		volTable[i] = envVolTable[2 * i + 1];
	}
	if (ay8910) {
		// only 16 envelope steps, duplicate every step
		envVolTable[1] = 0;
		for (int i = 2; i < 32; i += 2) {
			envVolTable[i] = envVolTable[i + 1];
		}
	}
}

inline unsigned AY8910::Amplitude::getEnvelopeMask() const
{
	return envChan[0] | (envChan[1] << 1) | (envChan[2] << 2);
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
		alternate = attack;
	} else {
		hold = shape & 0x01;
		alternate = shape & 0x02;
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

inline unsigned AY8910::Envelope::getNextEventTime()
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

AY8910::AY8910(MSXMotherBoard& motherBoard, AY8910Periphery& periphery_,
               const XMLElement& config, const EmuTime& time)
	: SoundDevice(motherBoard.getMSXMixer(), "PSG", "PSG", 3)
	, Resample(motherBoard.getGlobalSettings(), 1)
	, cliComm(motherBoard.getMSXCliComm())
	, periphery(periphery_)
	, debuggable(new AY8910Debuggable(motherBoard, *this))
	, amplitude(config)
	, envelope(amplitude.getEnvVolTable())
	, warningPrinted(false)
{
	initDetune();

	for (int chan = 0; chan < 3; chan++) {
		tone[chan].setParent(*this);
	}

	// make valgrind happy
	memset(regs, 0, sizeof(regs));
	setOutputRate(44100);

	const string& name = getName();
	CommandController& commandController = motherBoard.getCommandController();
	vibratoPercent.reset(new FloatSetting(commandController,
		name + "_vibrato_percent", "controls strength of vibrato effect",
		0.0, 0.0, 10.0));
	vibratoFrequency.reset(new FloatSetting(commandController,
		name + "_vibrato_frequency", "frequency of vibrato effect in Hertz",
		5, 1.0, 10.0));
	detunePercent.reset(new FloatSetting(commandController,
		name + "_detune_percent", "controls strength of detune effect",
		0.0, 0.0, 10.0));
	detuneFrequency.reset(new FloatSetting(commandController,
		name + "_detune_frequency", "frequency of detune effect in Hertz",
		5.0, 1.0, 100.0));

	reset(time);
	registerSound(config);
}

AY8910::~AY8910()
{
	unregisterSound();
}

void AY8910::reset(const EmuTime& time)
{
	// Reset generators and envelope.
	for (unsigned chan = 0; chan < 3; ++chan) {
		tone[chan].reset(0);
	}
	noise.reset();
	envelope.reset();
	// Reset registers and values derived from them.
	for (unsigned reg = 0; reg <= 15; ++reg) {
		wrtReg(reg, 0, time);
	}
}


byte AY8910::readRegister(unsigned reg, const EmuTime& time)
{
	assert(reg <= 15);
	switch (reg) {
	case AY_PORTA:
		if (!(regs[AY_ENABLE] & PORT_A_DIRECTION)) { //input
			regs[reg] = periphery.readA(time);
		}
		break;
	case AY_PORTB:
		if (!(regs[AY_ENABLE] & PORT_B_DIRECTION)) { //input
			regs[reg] = periphery.readB(time);
		}
		break;
	}
	return regs[reg];
}

byte AY8910::peekRegister(unsigned reg, const EmuTime& time) const
{
	assert(reg <= 15);
	switch (reg) {
	case AY_PORTA:
		if (!(regs[AY_ENABLE] & PORT_A_DIRECTION)) { //input
			return periphery.readA(time);
		}
		break;
	case AY_PORTB:
		if (!(regs[AY_ENABLE] & PORT_B_DIRECTION)) { //input
			return periphery.readB(time);
		}
		break;
	}
	return regs[reg];
}


void AY8910::writeRegister(unsigned reg, byte value, const EmuTime& time)
{
	assert(reg <= 15);
	if ((reg < AY_PORTA) && (reg == AY_ESHAPE || regs[reg] != value)) {
		// Update the output buffer before changing the register.
		updateStream(time);
	}
	wrtReg(reg, value, time);
}
void AY8910::wrtReg(unsigned reg, byte value, const EmuTime& time)
{
	// Warn/force port directions
	if (reg == AY_ENABLE) {
		if ((value & PORT_A_DIRECTION) && !warningPrinted) {
			warningPrinted = true;
			cliComm.printWarning(
				"The running MSX software has set unsafe PSG "
				"port directions (port A is set as output). "
				"This is not allowed by the MSX standard. "
				"Some MSX models (mostly MSX1) can get damaged "
				"by this.");
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

void AY8910::setOutputRate(unsigned sampleRate)
{
	setInputRate(NATIVE_FREQ_INT);
	setResampleRatio(NATIVE_FREQ_DOUBLE, sampleRate);
}

static void addFill(int*& buf, int val, unsigned num)
{
	// Note: in the past we tried to optimize this by always producing
	// a multiple of 4 output values. In the general case a sounddevice is
	// allowed to do this, but only at the of the soundbuffer. This method
	// cab be called multiple times per buffer, in such case it does go
	// wrong.
	assert(num > 0);
#ifdef __arm__
	asm volatile (
		"subs	%[num],%[num],#4\n\t"
		"bmi	1f\n"
	"0:\n\t"
		"ldmia	%[buf],{r3-r6}\n\t"
		"add	r3,r3,%[val]\n\t"
		"add	r4,r4,%[val]\n\t"
		"add	r5,r5,%[val]\n\t"
		"add	r6,r6,%[val]\n\t"
		"stmia	%[buf]!,{r3-r6}\n\t"
		"subs	%[num],%[num],#4\n\t"
		"bpl	0b\n"
	"1:\n\t"
		"tst	%[num],#2\n\t"
		"beq	2f\n\t"
		"ldmia	%[buf],{r3-r4}\n\t"
		"add	r3,r3,%[val]\n\t"
		"add	r4,r4,%[val]\n\t"
		"stmia	%[buf]!,{r3-r4}\n"
	"2:\n\t"
		"tst	%[num],#1\n\t"
		"beq	3f\n\t"
		"ldr	r3,[%[buf]]\n\t"
		"add	r3,r3,%[val]\n\t"
		"str	r3,[%[buf]],#4\n"
	"3:\n\t"
		: [buf] "=r"    (buf)
		, [num] "=r"    (num)
		:       "[buf]" (buf)
		, [val] "r"     (val)
		,       "[num]" (num)
		: "r3","r4","r5","r6"
	);
	return;
#endif
	do {
		*buf++ += val;
	} while (--num);
}

void AY8910::generateChannels(int** bufs, unsigned length)
{
	bool doDetune = (vibratoPercent->getValue() != 0) ||
	                (detunePercent->getValue()  != 0);

	// Disable channels with volume 0: since the sample value doesn't matter,
	// we can use the fastest path.
	unsigned chanEnable = regs[AY_ENABLE];
	for (unsigned chan = 0; chan < 3; ++chan) {
		if ((!amplitude.followsEnvelope(chan) &&
		     (amplitude.getVolume(chan) == 0)) ||
		    (amplitude.followsEnvelope(chan) &&
		     !envelope.isChanging() &&
		     (envelope.getVolume() == 0))) {
			bufs[chan] = 0;
			tone[chan].advance(length);
			chanEnable |= 0x09 << chan;
		}
	}
	// Noise disabled on all channels?
	if ((chanEnable & 0x38) == 0x38) {
		noise.advance(length);
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
				unsigned remaining = length;
				unsigned nextE = envelope.getNextEventTime();
				unsigned nextT = t.getNextEventTime();
				while ((nextT <= remaining) || (nextE <= remaining)) {
					if (nextT < nextE) {
						addFill(buf, val, nextT);
						remaining -= nextT;
						nextE -= nextT;
						envelope.advanceFast(nextT);
						t.doNextEvent(doDetune);
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
						t.doNextEvent(doDetune);
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
				unsigned remaining = length;
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
				t.advance(length);

			} else if ((chanEnable & 0x09) == 0x00) {
				// noise enabled, tone enabled
				noise = initialNoise;
				unsigned val = noise.getOutput() * t.getOutput() * envelope.getVolume();
				unsigned remaining = length;
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
						t.doNextEvent(doDetune);
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
				unsigned remaining = length;
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
				t.advance(length);
			}
		} else {
			// no (changing) envelope on this channel
			unsigned volume = amplitude.followsEnvelope(chan)
			                ? envelope.getVolume()
			                : amplitude.getVolume(chan);
			if ((chanEnable & 0x09) == 0x08) {
				// no noise, square wave: alternating between 0 and 1.
				unsigned val = t.getOutput() * volume;
				unsigned remaining = length;
				unsigned next = t.getNextEventTime();
				while (next <= remaining) {
					addFill(buf, val, next);
					val ^= volume;
					remaining -= next;
					t.doNextEvent(doDetune);
					next = t.getNextEventTime();
				}
				if (remaining) {
					// last interval (without events)
					addFill(buf, val, remaining);
					t.advanceFast(remaining);
				}

			} else if ((chanEnable & 0x09) == 0x09) {
				// no noise, channel disabled: always 1.
				addFill(buf, volume, length);
				t.advance(length);

			} else if ((chanEnable & 0x09) == 0x00) {
				// noise enabled, tone enabled
				noise = initialNoise;
				unsigned val1 = t.getOutput() * volume;
				unsigned val2 = val1 * noise.getOutput();
				unsigned remaining = length;
				unsigned nextN = noise.getNextEventTime();
				unsigned nextT = t.getNextEventTime();
				while ((nextN <= remaining) || (nextT <= remaining)) {
					if (nextT < nextN) {
						addFill(buf, val2, nextT);
						remaining -= nextT;
						nextN -= nextT;
						noise.advanceFast(nextT);
						t.doNextEvent(doDetune);
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
						t.doNextEvent(doDetune);
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
				unsigned remaining = length;
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
				t.advance(length);
			}
		}
	}

	// Envelope not yet updated?
	if (envelope.isChanging() && !envelopeUpdated) {
		envelope.advance(length);
	}
}

bool AY8910::generateInput(int* buffer, unsigned num)
{
	return mixChannels(buffer, num);
}

bool AY8910::updateBuffer(unsigned length, int* buffer,
     const EmuTime& /*time*/, const EmuDuration& /*sampDur*/)
{
	return generateOutput(buffer, length);
}


// SimpleDebuggable

AY8910Debuggable::AY8910Debuggable(MSXMotherBoard& motherBoard, AY8910& ay8910_)
	: SimpleDebuggable(motherBoard, ay8910_.getName() + " regs",
	                   "PSG", 0x10)
	, ay8910(ay8910_)
{
}

byte AY8910Debuggable::read(unsigned address, const EmuTime& time)
{
	return ay8910.readRegister(address, time);
}

void AY8910Debuggable::write(unsigned address, byte value, const EmuTime& time)
{
	return ay8910.writeRegister(address, value, time);
}

} // namespace openmsx
