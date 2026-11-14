#ifndef Y8960SSGCORE_HH
#define Y8960SSGCORE_HH

#include "FloatSetting.hh"
#include "TclCallback.hh"

#include "EmuTime.hh"

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace openmsx {

class AY8910Periphery;
class DeviceConfig;

/** One YM2149 of the Y8960's SSGS block.
  *
  * Forked from AY8910. It is not a SoundDevice of its own: Y8960SSGS owns two
  * of these and mixes them into one stereo stream, because the panpot is per
  * channel and openMSX only has per-device balance.
  *
  * The YMZ parts have no I/O ports, so registers 14 and 15 are dead storage
  * unless a periphery is given. The Y8960 gives none; the parameter stays so
  * that this keeps following AY8910.
  */
class Y8960SsgCore final
{
public:
	/** @param periphery_ what registers 14 and 15 drive, or nullptr for a
	  *                    unit without I/O ports. */
	Y8960SsgCore(const std::string& name, const DeviceConfig& config,
	             EmuTime time, AY8910Periphery* periphery_ = nullptr);
	~Y8960SsgCore();

	/** Generate 3 mono channels. Y8960SSGS applies the panpot afterwards. */
	void generateChannels(std::span<float*> bufs, unsigned num);
	[[nodiscard]] float getAmplificationFactor() const;

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

		void doNextEvent(const Y8960SsgCore& ay8910);

		/** Gets the current output of this generator.
		  */
		[[nodiscard]] bool getOutput() const { return output; }

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		[[nodiscard]] int getDetune(const Y8960SsgCore& ay8910);

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

	void wrtReg(unsigned reg, uint8_t value, EmuTime time);

	/** vibrato/detune are settings the user can change at any time, so the
	  * cached flag is refreshed once per buffer instead of being observed. */
	void updateDetune();

private:
	AY8910Periphery* const periphery;
	TclCallback directionsCallback;
	FloatSetting vibratoPercent;
	FloatSetting vibratoFrequency;
	FloatSetting detunePercent;
	FloatSetting detuneFrequency;
	std::array<ToneGenerator, 3> tone;
	NoiseGenerator noise;
	Amplitude amplitude;
	Envelope envelope;
	std::array<uint8_t, 16> regs;
	static constexpr bool isAY8910 = false; // the SSGS uses YM2149 cores
	const bool ignorePortDirections;
	bool doDetune;
};

SERIALIZE_CLASS_VERSION(Y8960SsgCore::Generator, 2);
SERIALIZE_CLASS_VERSION(Y8960SsgCore::ToneGenerator, 2);
SERIALIZE_CLASS_VERSION(Y8960SsgCore::NoiseGenerator, 2);

} // namespace openmsx

#endif // Y8960SSGCORE_HH
