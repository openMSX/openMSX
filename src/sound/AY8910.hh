#ifndef AY8910_HH
#define AY8910_HH

#include "ResampledSoundDevice.hh"
#include "FloatSetting.hh"
#include "SimpleDebuggable.hh"
#include "TclCallback.hh"
#include "openmsx.hh"

namespace openmsx {

class AY8910Periphery;
class DeviceConfig;

/** This class implements the AY-3-8910 sound chip.
  * Only the AY-3-8910 is emulated, no surrounding hardware,
  * use the class AY8910Periphery to connect peripherals.
  */
class AY8910 final : public ResampledSoundDevice
{
public:
	AY8910(const std::string& name, AY8910Periphery& periphery,
	       const DeviceConfig& config, EmuTime::param time);
	~AY8910();

	[[nodiscard]] byte readRegister(unsigned reg, EmuTime::param time);
	[[nodiscard]] byte peekRegister(unsigned reg, EmuTime::param time) const;
	void writeRegister(unsigned reg, byte value, EmuTime::param time);
	void reset(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Generator {
	public:
		inline void setPeriod(int value);
		[[nodiscard]] inline unsigned getNextEventTime() const;
		inline void advanceFast(unsigned duration);

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	protected:
		Generator() = default;
		inline void reset();

		/** Time between output steps.
		  * For tones, this is half the period of the square wave.
		  * For noise, this is the time before the random generator produces
		  * its next output.
		  */
		int period;
		/** Time passed in this period.
		  * Usually count will be smaller than period, but when the period
		  * was recently changed this might not be the case.
		  */
		int count;
	};

	class ToneGenerator : public Generator {
	public:
		ToneGenerator();

		inline void reset();

		/** Advance tone generator several steps in time.
		  * @param duration Length of interval to simulate.
		  */
		inline void advance(int duration);

		inline void doNextEvent(AY8910& ay8910);

		/** Gets the current output of this generator.
		  */
		[[nodiscard]] bool getOutput() const { return output; }

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		[[nodiscard]] int getDetune(AY8910& ay8910);

	private:
		/** Time passed since start of vibrato cycle.
		  */
		unsigned vibratoCount = 0;
		unsigned detuneCount = 0;

		/** Current state of the wave.
		  */
		bool output;
	};

	class NoiseGenerator : public Generator {
	public:
		NoiseGenerator();

		inline void reset();
		/** Advance noise generator several steps in time.
		  * @param duration Length of interval to simulate.
		  */
		inline void advance(int duration);

		inline void doNextEvent();

		/** Gets the current output of this generator.
		  */
		[[nodiscard]] bool getOutput() const { return random & 1; }

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		int random;
	};

	class Amplitude {
	public:
		explicit Amplitude(const DeviceConfig& config);
		[[nodiscard]] const float* getEnvVolTable() const;
		[[nodiscard]] inline float getVolume(unsigned chan) const;
		inline void setChannelVolume(unsigned chan, unsigned value);
		[[nodiscard]] inline bool followsEnvelope(unsigned chan) const;

	private:
		const float* envVolTable;
		float vol[3];
		bool envChan[3];
		const bool isAY8910;
	};

	class Envelope {
	public:
		explicit inline Envelope(const float* envVolTable);
		inline void reset();
		inline void setPeriod(int value);
		inline void setShape(unsigned shape);
		[[nodiscard]] inline bool isChanging() const;
		inline void advance(int duration);
		[[nodiscard]] inline float getVolume() const;

		[[nodiscard]] inline unsigned getNextEventTime() const;
		inline void advanceFast(unsigned duration);
		inline void doNextEvent();

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		inline void doSteps(int steps);

	private:
		const float* envVolTable;
		int period;
		int count;
		int step;
		int attack;
		bool hold, alternate, holding;
	};

	// SoundDevice
	void generateChannels(float** bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	void wrtReg(unsigned reg, byte value, EmuTime::param time);

private:
	AY8910Periphery& periphery;

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debuggable;

	FloatSetting vibratoPercent;
	FloatSetting vibratoFrequency;
	FloatSetting detunePercent;
	FloatSetting detuneFrequency;
	TclCallback directionsCallback;
	ToneGenerator tone[3];
	NoiseGenerator noise;
	Amplitude amplitude;
	Envelope envelope;
	byte regs[16];
	const bool isAY8910;
	const bool ignorePortDirections;
	bool doDetune;
	bool detuneInitialized;
};

SERIALIZE_CLASS_VERSION(AY8910::Generator, 2);
SERIALIZE_CLASS_VERSION(AY8910::ToneGenerator, 2);
SERIALIZE_CLASS_VERSION(AY8910::NoiseGenerator, 2);

} // namespace openmsx

#endif // AY8910_HH
