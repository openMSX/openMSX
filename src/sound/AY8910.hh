#ifndef AY8910_HH
#define AY8910_HH

#include "ResampledSoundDevice.hh"

#include "FloatSetting.hh"
#include "SimpleDebuggable.hh"
#include "TclCallback.hh"

#include <array>
#include <cstdint>

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
	       const DeviceConfig& config, EmuTime time);
	~AY8910();

	[[nodiscard]] uint8_t readRegister(unsigned reg, EmuTime time);
	[[nodiscard]] uint8_t peekRegister(unsigned reg, EmuTime time) const;
	void writeRegister(unsigned reg, uint8_t value, EmuTime time);
	void reset(EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Generator {
	public:
		void setPeriod(int value);
		[[nodiscard]] unsigned getNextEventTime() const;
		void advanceFast(unsigned duration);

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	protected:
		Generator() = default;
		void reset();

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

		void reset();

		/** Advance tone generator several steps in time.
		  * @param duration Length of interval to simulate.
		  */
		void advance(unsigned duration);

		void doNextEvent(const AY8910& ay8910);

		/** Gets the current output of this generator.
		  */
		[[nodiscard]] bool getOutput() const { return output; }

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		[[nodiscard]] int getDetune(const AY8910& ay8910);

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

		void reset();
		/** Advance noise generator several steps in time.
		  * @param duration Length of interval to simulate.
		  */
		void advance(unsigned duration);

		void doNextEvent();

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
		[[nodiscard]] auto getEnvVolTable() const { return envVolTable; }
		[[nodiscard]] float getVolume(unsigned chan) const;
		void setChannelVolume(unsigned chan, unsigned value);
		[[nodiscard]] bool followsEnvelope(unsigned chan) const;

	private:
		const bool isAY8910; // must come before envVolTable
		std::span<const float, 32> envVolTable;
		std::array<float, 3> vol;
		std::array<bool, 3> envChan;
	};

	class Envelope {
	public:
		explicit Envelope(std::span<const float, 32> envVolTable);
		void reset();
		void setPeriod(int value);
		void setShape(unsigned shape);
		[[nodiscard]] bool isChanging() const;
		void advance(unsigned duration);
		[[nodiscard]] float getVolume() const;

		[[nodiscard]] unsigned getNextEventTime() const;
		void advanceFast(unsigned duration);
		void doNextEvent();

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		void doSteps(int steps);

	private:
		std::span<const float, 32> envVolTable;
		int period = 1;
		int count = 0;
		int step = 0;
		int attack = 0;
		bool hold = false, alternate = false, holding = false;
	};

	// SoundDevice
	void generateChannels(std::span<float*> bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	void wrtReg(unsigned reg, uint8_t value, EmuTime time);

private:
	AY8910Periphery& periphery;

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} debuggable;

	FloatSetting vibratoPercent;
	FloatSetting vibratoFrequency;
	FloatSetting detunePercent;
	FloatSetting detuneFrequency;
	TclCallback directionsCallback;
	std::array<ToneGenerator, 3> tone;
	NoiseGenerator noise;
	Amplitude amplitude;
	Envelope envelope;
	std::array<uint8_t, 16> regs;
	const bool isAY8910;
	const bool ignorePortDirections;
	bool doDetune;
};

SERIALIZE_CLASS_VERSION(AY8910::Generator, 2);
SERIALIZE_CLASS_VERSION(AY8910::ToneGenerator, 2);
SERIALIZE_CLASS_VERSION(AY8910::NoiseGenerator, 2);

} // namespace openmsx

#endif // AY8910_HH
