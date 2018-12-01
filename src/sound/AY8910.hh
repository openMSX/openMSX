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

	byte readRegister(unsigned reg, EmuTime::param time);
	byte peekRegister(unsigned reg, EmuTime::param time) const;
	void writeRegister(unsigned reg, byte value, EmuTime::param time);
	void reset(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Generator {
	public:
		inline void setPeriod(int value);
		inline unsigned getNextEventTime() const;
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
		bool getOutput() const { return output; }

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		int getDetune(AY8910& ay8910);

		/** Time passed since start of vibrato cycle.
		  */
		unsigned vibratoCount;
		unsigned detuneCount;

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
		bool getOutput() const { return random & 1; }

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		int random;
	};

	class Amplitude {
	public:
		explicit Amplitude(const DeviceConfig& config);
		const unsigned* getEnvVolTable() const;
		inline unsigned getVolume(unsigned chan) const;
		inline void setChannelVolume(unsigned chan, unsigned value);
		inline void setMasterVolume(int volume);
		inline bool followsEnvelope(unsigned chan) const;

	private:
		unsigned volTable[16];
		unsigned envVolTable[32];
		unsigned vol[3];
		bool envChan[3];
		const bool isAY8910;
	};

	class Envelope {
	public:
		explicit inline Envelope(const unsigned* envVolTable);
		inline void reset();
		inline void setPeriod(int value);
		inline void setShape(unsigned shape);
		inline bool isChanging() const;
		inline void advance(int duration);
		inline unsigned getVolume() const;

		inline unsigned getNextEventTime() const;
		inline void advanceFast(unsigned duration);
		inline void doNextEvent();

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		inline void doSteps(int steps);

		const unsigned* envVolTable;
		int period;
		int count;
		int step;
		int attack;
		bool hold, alternate, holding;
	};

	// SoundDevice
	void generateChannels(int** bufs, unsigned num) override;

	// Observer<Setting>
	void update(const Setting& setting) override;

	void wrtReg(unsigned reg, byte value, EmuTime::param time);

	AY8910Periphery& periphery;

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		byte read(unsigned address, EmuTime::param time) override;
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
	bool doDetune;
	bool detuneInitialized;
};

SERIALIZE_CLASS_VERSION(AY8910::Generator, 2);
SERIALIZE_CLASS_VERSION(AY8910::ToneGenerator, 2);
SERIALIZE_CLASS_VERSION(AY8910::NoiseGenerator, 2);

} // namespace openmsx

#endif // AY8910_HH
