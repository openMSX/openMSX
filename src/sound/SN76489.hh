#ifndef SN76489_HH
#define SN76489_HH

#include "ResampledSoundDevice.hh"
#include "SimpleDebuggable.hh"

namespace openmsx {

/** This class implements the Texas Instruments SN76489 sound chip.
  * Unlike the AY-3-8910, this chip only performs sound synthesis.
  *
  * Resources used:
  * - SN76489AN data sheet
  *   http://map.grauw.nl/resources/sound/texas_instruments_sn76489an.pdf
  * - Developer documentation on SMS Power!
  *   http://www.smspower.org/Development/SN76489
  * - John Kortink's reverse engineering results
  *   http://www.zeridajh.org/articles/various/sn76489/index.htm
  * - MAME's implementation
  *   https://github.com/mamedev/mame/blob/master/src/devices/sound/sn76496.cpp
  * - blueMSX's implementation
  *   https://sourceforge.net/p/bluemsx/code/HEAD/tree/trunk/blueMSX/Src/SoundChips/SN76489.c
  */
class SN76489 final : public ResampledSoundDevice
{
public:
	SN76489(const DeviceConfig& config);
	~SN76489();

	// ResampledSoundDevice
	void generateChannels(float** buffers, unsigned num) override;

	void reset(EmuTime::param time);
	void write(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class NoiseShifter {
	public:
		/** Sets the feedback pattern and resets the shift register to the
		  * initial state that matches that pattern.
		  */
		inline void initState(unsigned pattern, unsigned period);

		/** Gets the current output of this shifter: 0 or 1.
		  */
		[[nodiscard]] inline unsigned getOutput() const;

		/** Advances the shift register one step, to the next output.
		  */
		inline void advance();

		/** Records that the shift register should be advanced multiple steps
		  * before the next output is used.
		  */
		inline void queueAdvance(unsigned steps);

		/** Advances the shift register by the number of steps that were queued.
		  */
		void catchUp();

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		unsigned pattern;
		unsigned period;
		unsigned random;
		unsigned stepsBehind;
	};

	/** Initialize registers, counters and other chip state.
	  */
	void initState();

	/** Initialize noise shift register according to chip variant and noise
	  * mode.
	  */
	void initNoise();

	[[nodiscard]] word peekRegister(unsigned reg, EmuTime::param time) const;
	void writeRegister(unsigned reg, word value, EmuTime::param time);
	template<bool NOISE> void synthesizeChannel(
		float*& buffer, unsigned num, unsigned generator);

private:
	NoiseShifter noiseShifter;

	/** Register bank. The tone period registers (0, 2, 4) are 12 bits wide,
	  * all other registers are 4 bits wide.
	  */
	word regs[8];

	/** The last register written to (0-7).
	  */
	byte registerLatch;

	/** Frequency counter for each channel.
	  * These count down and when they reach zero, the output flips and the
	  * counter is re-loaded with the value of the period register.
	  */
	word counters[4];

	/** Output flip-flop state (0 or 1) for each channel.
	  */
	byte outputs[4];

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debuggable;
};

} // namespace openmsx

#endif
