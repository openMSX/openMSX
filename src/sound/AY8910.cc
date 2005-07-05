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
#include "MSXMotherBoard.hh"
#include "Mixer.hh"
#include "Debugger.hh"
#include "Scheduler.hh"
#include "AY8910Periphery.hh"
#include <cassert>

using std::string;

namespace openmsx {

static const bool FORCE_PORTA_INPUT = true;

// Fixed point representation of 1.
static const int FP_UNIT = 0x8000;

static const int CLOCK = 3579545 / 2;
static const int PORT_A_DIRECTION = 0x40;
static const int PORT_B_DIRECTION = 0x80;
enum Register {
	AY_AFINE = 0, AY_ACOARSE = 1, AY_BFINE = 2, AY_BCOARSE = 3,
	AY_CFINE = 4, AY_CCOARSE = 5, AY_NOISEPER = 6, AY_ENABLE = 7,
	AY_AVOL = 8, AY_BVOL = 9, AY_CVOL = 10, AY_EFINE = 11,
	AY_ECOARSE = 12, AY_ESHAPE = 13, AY_PORTA = 14, AY_PORTB = 15
};


// Generator:

AY8910::Generator::Generator()
{
	reset();
}

inline void AY8910::Generator::reset(byte output)
{
	period = 0;
	count = 0;
	this->output = output;
}

inline void AY8910::Generator::setPeriod(int value, unsigned int updateStep)
{
	// A note about the period of tones, noise and envelope: for speed
	// reasons, we count down from the period to 0, but careful studies of the
	// chip output prove that it instead counts up from 0 until the counter
	// becomes greater or equal to the period. This is an important difference
	// when the program is rapidly changing the period to modulate the sound.
	// To compensate for the difference, when the period is changed we adjust
	// our internal counter.
	// Also, note that period = 0 is the same as period = 1. This is mentioned
	// in the YM2203 data sheets. However, this does NOT apply to the Envelope
	// period. In that case, period = 0 is half as period = 1.
	const int old = period;
	if (value == 0) value = 1;
	period = value * updateStep;
	count += period - old;
	if (count <= 0) count = 1;
}


// ToneGenerator:

template <bool enabled>
inline int AY8910::ToneGenerator::advance(int duration)
{
	int highDuration = 0;
	if (enabled && output) highDuration += count;
	count -= duration;
	if (count <= 0) {
		// Calculate number of output transitions.
		int cycles = 1 + (-count) / period;
		if (enabled) {
			// Full square waves: output is 1 half of the time;
			// which half doesn't matter.
			highDuration += period * (cycles / 2);
		}
		if (cycles & 1) {
			// Half a square wave.
			output ^= 1;
			if (enabled && output) highDuration += period;
		}
		count += period * cycles;
	}
	if (enabled && output) highDuration -= count;
	return highDuration;
}


// NoiseGenerator:

AY8910::NoiseGenerator::NoiseGenerator()
{
	reset();
}

inline void AY8910::NoiseGenerator::reset()
{
	Generator::reset(0x38);
	random = 1;
}

inline byte AY8910::NoiseGenerator::getOutput()
{
	return output;
}

inline int AY8910::NoiseGenerator::advanceToFlip(int duration)
{
	int left = duration;
	while (true) {
		if (count > left) {
			// Exit: end of interval.
			count -= left;
			return duration;
		}
		left -= count;
		count = period;
		// Is noise output going to change?
		const bool flip = (random + 1) & 2; // bit0 ^ bit1
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
		random =  (random >> 1)
		       ^ ((random & 1) << 14)
		       ^ ((random & 1) << 16);
		if (flip) {
			// Exit: output flip.
			output ^= 0x38;
			return duration - left;
		}
	}
}

inline void AY8910::NoiseGenerator::advance(int duration)
{
	int cycles = (duration + period - count) / period;
	count += cycles * period - duration;
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
	output = (random & 1) ? 0x38 : 0x00;
}


// Amplitude:

AY8910::Amplitude::Amplitude()
{
	vol[0] = vol[1] = vol[2] = 0;
	envChan[0] = envChan[1] = envChan[2] = false;
	envVolume = 0;
	setMasterVolume(0); // avoid UMR
}

inline unsigned int AY8910::Amplitude::getVolume(byte chan)
{
	return vol[chan];
}

inline void AY8910::Amplitude::setChannelVolume(byte chan, byte value)
{
	envChan[chan] = value & 0x10;
	vol[chan] = envChan[chan] ? envVolume : volTable[value & 0x0F];
}

inline void AY8910::Amplitude::setEnvelopeVolume(byte volume)
{
	envVolume = volTable[volume];
	if (envChan[0]) vol[0] = envVolume;
	if (envChan[1]) vol[1] = envVolume;
	if (envChan[2]) vol[2] = envVolume;
}

inline void AY8910::Amplitude::setMasterVolume(int volume)
{
	// Calculate the volume->voltage conversion table.
	// The AY-3-8910 has 16 levels, in a logarithmic scale (3dB per step).
	double out = volume; // avoid clipping
	for (int i = 15; i > 0; --i) {
		volTable[i] = (unsigned)(out + 0.5);	// round to nearest
		out *= 0.707945784384;			// 1/(10^(3/20)) = 1/(3dB)
	}
	volTable[0] = 0;
}

inline bool AY8910::Amplitude::anyEnvelope()
{
	return envChan[0] || envChan[1] || envChan[2];
}


// Envelope:

inline AY8910::Envelope::Envelope(Amplitude& amplitude)
	: amplitude(amplitude)
{
	period = count = step = attack = 0;
	hold = alternate = holding = false;
}

inline void AY8910::Envelope::reset()
{
	period = 0;
	count = 0;
}

inline void AY8910::Envelope::setPeriod(int value, unsigned int updateStep)
{
	const int old = period;
	if (value == 0) {
		period = updateStep;
	} else {
		period = value * 2 * updateStep;
	}
	count += period - old;
	if (count <= 0) count = 1;
}

inline byte AY8910::Envelope::getVolume()
{
	return step ^ attack;
}

inline void AY8910::Envelope::setShape(byte shape)
{
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
	attack = (shape & 0x04) ? 0x0F : 0x00;
	if ((shape & 0x08) == 0) {
		// If Continue = 0, map the shape to the equivalent one
		// which has Continue = 1.
		hold = true;
		alternate = attack;
	} else {
		hold = shape & 0x01;
		alternate = shape & 0x02;
	}
	count = period;
	step = 0x0F;
	holding = false;
	amplitude.setEnvelopeVolume(getVolume());
}

inline bool AY8910::Envelope::isChanging()
{
	return !holding;
}

inline void AY8910::Envelope::advance(int duration)
{
	if (!holding) {
		count -= duration;
		if (count <= 0) {
			const int steps = 1 + (-count) / period;
			step -= steps;
			count += steps * period;

			// Check current envelope position.
			if (step < 0) {
				if (hold) {
					if (alternate) attack ^= 0x0f;
					holding = true;
					step = 0;
				} else {
					// If step has looped an odd number of times
					// (usually 1), invert the output.
					if (alternate && (step & 0x10)) {
						attack ^= 0x0F;
					}
					step &= 0x0F;
				}
			}
			amplitude.setEnvelopeVolume(getVolume());
		}
	}
}


// AY8910 main class:

AY8910::AY8910(MSXMotherBoard& motherBoard, AY8910Periphery& periphery_,
               const XMLElement& config, const EmuTime& time)
	: SoundDevice(motherBoard.getMixer())
	, periphery(periphery_)
	, debugger(motherBoard.getDebugger())
	, envelope(amplitude)
{
	// make valgrind happy
	memset(regs, 0, sizeof(regs));
	setSampleRate(44100);

	reset(time);
	registerSound(config);
	debugger.registerDebuggable(getName() + " regs", *this);
}

AY8910::~AY8910()
{
	debugger.unregisterDebuggable(getName() + " regs", *this);
	unregisterSound();
}

const string& AY8910::getName() const
{
	static const string name("PSG");
	return name;
}

const string& AY8910::getDescription() const
{
	static const string desc("PSG");
	return desc;
}

void AY8910::reset(const EmuTime& time)
{
	// Reset generators and envelope.
	for (byte chan = 0; chan < 3; chan++) {
		tone[chan].reset();
	}
	noise.reset();
	envelope.reset();
	// Reset registers and values derived from them.
	oldEnable = 0;
	for (byte reg = 0; reg <= 15; reg++) {
		wrtReg(reg, 0, time);
	}
}


byte AY8910::readRegister(byte reg, const EmuTime& time)
{
	assert(reg <= 15);
	switch (reg) {
	case AY_PORTA:
		if (FORCE_PORTA_INPUT ||
		    !(regs[AY_ENABLE] & PORT_A_DIRECTION)) { //input
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

byte AY8910::peekRegister(byte reg, const EmuTime& time) const
{
	assert(reg <= 15);
	switch (reg) {
	case AY_PORTA:
		if (FORCE_PORTA_INPUT ||
		    !(regs[AY_ENABLE] & PORT_A_DIRECTION)) { //input
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


void AY8910::writeRegister(byte reg, byte value, const EmuTime& time)
{
	assert(reg <= 15);
	if ((reg < AY_PORTA) && (reg == AY_ESHAPE || regs[reg] != value)) {
		// Update the output buffer before changing the register.
		getMixer().updateStream(time);
	}
	getMixer().lock();
	wrtReg(reg, value, time);
	getMixer().unlock();
}
void AY8910::wrtReg(byte reg, byte value, const EmuTime& time)
{
	// Note: unused bits are stored as well; they can be read back.
	regs[reg] = value;

	switch (reg) {
	case AY_AFINE:
	case AY_ACOARSE:
	case AY_BFINE:
	case AY_BCOARSE:
	case AY_CFINE:
	case AY_CCOARSE:
		tone[reg / 2].setPeriod(
			regs[reg & ~1] + 256 * (regs[reg | 1] & 0x0F), updateStep );
		break;
	case AY_NOISEPER:
		noise.setPeriod(value & 0x1F, updateStep);
		break;
	case AY_AVOL:
	case AY_BVOL:
	case AY_CVOL:
		amplitude.setChannelVolume(reg - AY_AVOL, value);
		checkMute();
		break;
	case AY_EFINE:
	case AY_ECOARSE:
		envelope.setPeriod(
			regs[AY_EFINE] + 256 * regs[AY_ECOARSE], updateStep );
		break;
	case AY_ESHAPE:
		envelope.setShape(value);
		break;
	case AY_ENABLE:
		if (!FORCE_PORTA_INPUT &&
		    (value      & PORT_A_DIRECTION) &&
		    !(oldEnable & PORT_A_DIRECTION)) {
			// Changed from input to output.
			periphery.writeA(regs[AY_PORTA], time);
		}
		if ((value     & PORT_B_DIRECTION) &&
		    !(oldEnable & PORT_B_DIRECTION)) {
			// Changed from input to output.
			periphery.writeB(regs[AY_PORTB], time);
		}
		oldEnable = value;
		checkMute();
		break;
	case AY_PORTA:
		if (!FORCE_PORTA_INPUT &&
		    regs[AY_ENABLE] & PORT_A_DIRECTION) { // output
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

void AY8910::checkMute()
{
	setMute(((regs[AY_AVOL] | regs[AY_BVOL] | regs[AY_CVOL]) & 0x1F) == 0);
}

void AY8910::setVolume(int volume)
{
	amplitude.setMasterVolume(volume);
}


void AY8910::setSampleRate(int sampleRate)
{
	// The step clock for the tone and noise generators is the chip clock
	// divided by 8; for the envelope generator of the AY-3-8910, it is half
	// that much (clock/16).
	// Here we calculate the number of steps which happen during one sample
	// at the given sample rate. No. of events = sample rate / (clock/8).
	// FP_UNIT is a multiplier used to turn the fraction into a fixed point
	// number.

	// !! look out for overflow !!
	updateStep = (FP_UNIT * sampleRate) / (CLOCK / 8);
}


void AY8910::updateBuffer(unsigned length, int* buffer,
     const EmuTime& /*time*/, const EmuDuration& /*sampDur*/)
{
	/*
	static long long totalSamples = 0, noiseOff = 0, toneOff = 0, bothOff = 0;
	static long long envSamples = 0;
	static long long sameSample = 0;
	long long oldTotal = totalSamples;
	for (byte chan = 0; chan < 3; chan++) {
		totalSamples += length;
		if (regs[AY_ENABLE] & (0x08 << chan)) noiseOff += length;
		if (regs[AY_ENABLE] & (0x01 << chan)) toneOff += length;
		if ((regs[AY_ENABLE] & (0x09 << chan)) == (0x09 << chan)) {
			bothOff += length;
		}
	}
	if (amplitude.anyEnvelope()) envSamples += length;
	if ((totalSamples / 100000) != (oldTotal / 100000)) {
		fprintf(stderr,
			"%lld samples, %3.3f%% noise, %3.3f%% tone, %3.3f%% neither, "
			"%3.3f%% envelope, %3.3f%% same sample\n",
			totalSamples,
			100.0 * ((double)(totalSamples - noiseOff)/(double)totalSamples),
			100.0 * ((double)(totalSamples - toneOff)/(double)totalSamples),
			100.0 * ((double)bothOff/(double)totalSamples),
			100.0 * ((double)envSamples/(double)totalSamples),
			100.0 * ((double)sameSample/(double)totalSamples)
			);
	}
	*/

	byte chanEnable = regs[AY_ENABLE];
	// Disable channels with volume 0: since the sample value doesn't matter,
	// we can use the fastest path.
	for (byte chan = 0; chan < 3; chan++) {
		if ((regs[AY_AVOL + chan] & 0x1F) == 0) {
			chanEnable |= 0x09 << chan;
		}
	}
	// Advance tone generators for channels that have tone disabled.
	for (byte chan = 0; chan < 3; chan++) {
		if (chanEnable & (0x01 << chan)) { // disabled
			tone[chan].advance<false>(length * FP_UNIT);
		}
	}
	// Noise enabled on any channel?
	bool anyNoise = (chanEnable & 0x38) != 0x38;
	if (!anyNoise) {
		noise.advance(length * FP_UNIT);
	}
	// Envelope enabled on any channel?
	bool enveloping = amplitude.anyEnvelope() && envelope.isChanging();
	if (!enveloping) {
		envelope.advance(length * FP_UNIT);
	}

	// Calculate samples.
	while (length) {
		// semiVol keeps track of how long each square wave stays
		// in the 1 position during the sample period.
		int semiVol[3] = { 0, 0, 0 };
		int left = FP_UNIT;
		do {
			// The 8910 has three outputs, each output is the mix of one of
			// the three tone generators and of the (single) noise generator.
			// The two are mixed BEFORE going into the DAC. The formula to mix
			// each channel is:
			//   (ToneOn | ToneDisable) & (NoiseOn | NoiseDisable),
			//   where ToneOn and NoiseOn are the current generator state
			//   and ToneDisable and NoiseDisable come from the enable reg.
			// Note that this means that if both tone and noise are disabled,
			// the output is 1, not 0, and can be modulated by changing the
			// volume.

			// Update state of noise generator.
			byte chanFlags;
			int nextEvent;
			if (anyNoise) {
				// Next event is end of this sample or noise flip.
				chanFlags = noise.getOutput() | chanEnable;
				nextEvent = noise.advanceToFlip(left);
			} else {
				// Next event is end of this sample.
				chanFlags = chanEnable;
				nextEvent = FP_UNIT;
			}

			// Mix tone generators with noise generator.
			for (byte chan = 0; chan < 3; chan++, chanFlags >>= 1) {
				if ((chanFlags & 0x09) == 0x08) {
					// Square wave: alternating between 0 and 1.
					semiVol[chan] += tone[chan].advance<true>(nextEvent);
				} else if ((chanFlags & 0x09) == 0x09) {
					// Channel disabled: always 1.
					semiVol[chan] += nextEvent;
				} else if ((chanFlags & 0x09) == 0x00) {
					// Tone enabled, but suppressed by noise state.
					tone[chan].advance<false>(nextEvent);
				} else { // (chanFlags & 0x09) == 0x01
					// Tone disabled, noise state is 0.
					// Nothing to do.
				}
			}

			left -= nextEvent;
		} while (left > 0);

		// Update envelope.
		if (enveloping) envelope.advance(FP_UNIT);

		// Calculate D/A converter output.
		// TODO: Is it easy to detect when multiple samples have the same
		//       value? (make nextEvent depend on tone events as well?)
		//       At 44KHz, value typically changes once every 4 to 5 samples.
		int chA = (semiVol[0] * amplitude.getVolume(0)) / FP_UNIT;
		int chB = (semiVol[1] * amplitude.getVolume(1)) / FP_UNIT;
		int chC = (semiVol[2] * amplitude.getVolume(2)) / FP_UNIT;
		/*
		static int prevSample = 0;
		if (chA + chB + chC == prevSample) sameSample += 3;
		else prevSample = chA + chB + chC;
		*/
		*(buffer++) = chA + chB + chC;
		length--;
	}
}


// Debuggable

unsigned AY8910::getSize() const
{
	return 0x10;
}

byte AY8910::read(unsigned address)
{
	return readRegister(address, Scheduler::instance().getCurrentTime());
}

void AY8910::write(unsigned address, byte value)
{
	return writeRegister(address, value,
	                     Scheduler::instance().getCurrentTime());
}

} // namespace openmsx
