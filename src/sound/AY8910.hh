// $Id$

#ifndef AY8910_HH
#define AY8910_HH

#include "SoundDevice.hh"
#include "Resample.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class CliComm;
class AY8910Periphery;
class XMLElement;
class EmuTime;
class AY8910Debuggable;
class FloatSetting;

/** This class implements the AY-3-8910 sound chip.
  * Only the AY-3-8910 is emulated, no surrounding hardware,
  * use the class AY8910Periphery to connect peripherals.
  */
class AY8910 : public SoundDevice, private Resample
{
public:
	AY8910(MSXMotherBoard& motherBoard, AY8910Periphery& periphery_,
	       const XMLElement& config, const EmuTime& time);
	virtual ~AY8910();

	byte readRegister(unsigned reg, const EmuTime& time);
	byte peekRegister(unsigned reg, const EmuTime& time) const;
	void writeRegister(unsigned reg, byte value, const EmuTime& time);
	void reset(const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Generator {
	public:
		inline void reset(unsigned output);
		inline void setPeriod(int value);
		/** Gets the current output of this generator.
		  */
		inline unsigned getOutput();

		inline unsigned getNextEventTime();
		inline void advanceFast(unsigned duration);

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	protected:
		Generator();

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
		/** Current state of the wave.
		  * For tones, this is 0 or 1.
		  */
		unsigned output;
	};

	class ToneGenerator: public Generator {
	public:
		ToneGenerator();
		inline void setParent(AY8910& parent);
		/** Advance tone generator several steps in time.
		  * @param duration Length of interval to simulate.
		  */
		inline void advance(int duration);

		inline void doNextEvent(bool doDetune);

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		int getDetune();

		AY8910* parent;
		/** Time passed since start of vibrato cycle.
		  */
		unsigned vibratoCount;
		unsigned detuneCount;
	};

	class NoiseGenerator: public Generator {
	public:
		NoiseGenerator();

		inline void reset();
		/** Advance noise generator several steps in time.
		  * @param duration Length of interval to simulate.
		  */
		inline void advance(int duration);

		inline void doNextEvent();

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

	private:
		int random;
	};

	class Amplitude {
	public:
		explicit Amplitude(const XMLElement& config);
		const unsigned* getEnvVolTable() const;
		inline unsigned getVolume(unsigned chan) const;
		inline void setChannelVolume(unsigned chan, unsigned value);
		inline void setMasterVolume(int volume);
		inline unsigned getEnvelopeMask() const;
		inline bool followsEnvelope(unsigned chan) const;

	private:
		unsigned volTable[16];
		unsigned envVolTable[32];
		unsigned vol[3];
		bool envChan[3];
		bool ay8910;
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

		inline unsigned getNextEventTime();
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

	// SoundDevice:
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(unsigned length, int* buffer,
		const EmuTime& time, const EmuDuration& sampDur);

	// Resample
	virtual bool generateInput(int* buffer, unsigned num);

	void wrtReg(unsigned reg, byte value, const EmuTime& time);

	CliComm& cliComm;
	AY8910Periphery& periphery;
	const std::auto_ptr<AY8910Debuggable> debuggable;
	std::auto_ptr<FloatSetting> vibratoPercent;
	std::auto_ptr<FloatSetting> vibratoFrequency;
	std::auto_ptr<FloatSetting> detunePercent;
	std::auto_ptr<FloatSetting> detuneFrequency;
	ToneGenerator tone[3];
	NoiseGenerator noise;
	Amplitude amplitude;
	Envelope envelope;
	byte regs[16];
	bool warningPrinted;
};

} // namespace openmsx

#endif // AY8910_HH
