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
	period = value == 0 ? 1 : value;
}

inline unsigned AY8910::Generator::getOutput()
{
	return output;
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

inline int AY8910::ToneGenerator::getDetune()
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
	return result;
}

inline void AY8910::ToneGenerator::advance()
{
	count++;
	if (count >= period) {
		count = getDetune();
		output ^= 1;
	}
}

inline void AY8910::ToneGenerator::advance(int duration)
{
	int oldCount = count;
	count += duration;
	if (count >= period) {
		if (oldCount > period) {
			// this only happens when period was just changed,
			// this code makes sure the effect of this method is the
			// same as calling 'duration x advance()'
			count -= oldCount - period;
		}
		// Calculate number of output transitions.
		int cycles = count / period;
		count -= period * cycles; // equivalent to count %= period;
		output ^= cycles & 1;
	}
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

inline void AY8910::NoiseGenerator::advance()
{
	count++;
	if (count >= period) {
		count = 0;
		// Is noise output going to change?
		if ((random + 1) & 2) { // bit0 ^ bit1
			// Exit: output flip.
			output ^= 1;
		}
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
}

inline void AY8910::NoiseGenerator::advance(int duration)
{
	if (count > period) {
		// this only happens when period was just changed,
		// this code makes sure the effect of this method is the
		// same as calling 'duration x advance()'
		count = period;
	}
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
	period = 0;
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
	period = value == 0 ? 1 : value * 2;
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

inline void AY8910::Envelope::advance(int duration)
{
	// For best performance callers should check upfront whether
	//    isChanging() == true
	// Though we can't assert on it because the condition might change
	// in the inner loop(s) of generateChannels().
	//assert(!holding);

	int oldCount = count;
	count += duration * 2;
	if (count >= period) {
		if (oldCount > period) {
			// this only happens when period was just changed,
			// this code makes sure the effect of this method is the
			// same as calling 'duration x advance(1)'
			count -= oldCount - period;
		}
		if (holding) return;
		const int steps = count / period;
		step -= steps;
		count -= steps * period; // equivalent to count %= period;

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


void AY8910::generateChannels(int** bufs, unsigned length)
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
			bufs[chan] = 0;
			tone[chan].advance(length);
			chanEnable |= 0x09 << chan;
		}
	}
	// Noise disabled on all channels?
	if ((chanEnable & 0x38) == 0x38) {
		noise.advance(length);
	}
	// Envelope disabled on all channels?
	if (envelope.isChanging() &&
	    !(amplitude.getEnvelopeMask() & ~chanEnable)) {
		envelope.advance(length);
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
	Envelope initialEnvelope = envelope;
	NoiseGenerator initialNoise = noise;
	for (unsigned chan = 0; chan < 3; ++chan, chanEnable >>= 1) {
		int* buf = bufs[chan];
		if (!buf) continue;
		if (envelope.isChanging() && amplitude.followsEnvelope(chan)) {
			envelope = initialEnvelope;
			ToneGenerator& t = tone[chan];
			if ((chanEnable & 0x09) == 0x08) {
				// no noise, square wave: alternating between 0 and 1.
				for (unsigned i = 0; i < length; ++i) {
					buf[i] = t.getOutput() * envelope.getVolume();
					t.advance();
					envelope.advance(1);
				}
			} else if ((chanEnable & 0x09) == 0x09) {
				// no noise, channel disabled: always 1.
				for (unsigned i = 0; i < length; ++i) {
					buf[i] = envelope.getVolume();
					envelope.advance(1);
				}
				t.advance(length);
			} else if ((chanEnable & 0x09) == 0x00) {
				// noise enabled, tone enabled
				noise = initialNoise;
				for (unsigned i = 0; i < length; ++i) {
					buf[i] = noise.getOutput()
					       ? t.getOutput() * envelope.getVolume()
					       : 0;
					t.advance();
					noise.advance();
					envelope.advance(1);
				}
			} else {
				// noise enabled, tone disabled
				noise = initialNoise;
				for (unsigned i = 0; i < length; ++i) {
					buf[i] = noise.getOutput()
					       ? envelope.getVolume()
					       : 0;
					noise.advance();
					envelope.advance(1);
				}
				t.advance(length);
			}
		} else {
			// no (changing) envelope on this channel
			unsigned volume = amplitude.followsEnvelope(chan)
			                ? envelope.getVolume()
			                : amplitude.getVolume(chan);
			ToneGenerator& t = tone[chan];
			if ((chanEnable & 0x09) == 0x08) {
				// no noise, square wave: alternating between 0 and 1.
				for (unsigned i = 0; i < length; ++i) {
					buf[i] = t.getOutput() * volume;
					t.advance();
				}
			} else if ((chanEnable & 0x09) == 0x09) {
				// no noise, channel disabled: always 1.
				for (unsigned i = 0; i < length; ++i) {
					buf[i] = volume;
				}
				t.advance(length);
			} else if ((chanEnable & 0x09) == 0x00) {
				// noise enabled, tone enabled
				noise = initialNoise;
				for (unsigned i = 0; i < length; ++i) {
					buf[i] = noise.getOutput()
					       ? t.getOutput() * volume
					       : 0;
					t.advance();
					noise.advance();
				}
			} else {
				// noise enabled, tone disabled
				noise = initialNoise;
				for (unsigned i = 0; i < length; ++i) {
					buf[i] = noise.getOutput()
					       ? volume
					       : 0;
					noise.advance();
				}
				t.advance(length);
			}
		}
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
