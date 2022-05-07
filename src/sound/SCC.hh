#ifndef SCC_HH
#define SCC_HH

#include "ResampledSoundDevice.hh"
#include "SimpleDebuggable.hh"
#include "Clock.hh"
#include "openmsx.hh"

namespace openmsx {

class SCC final : public ResampledSoundDevice
{
public:
	enum ChipMode {SCC_Real, SCC_Compatible, SCC_plusmode};

	SCC(const std::string& name, const DeviceConfig& config,
	    EmuTime::param time, ChipMode mode = SCC_Real);
	~SCC();

	// interaction with realCartridge
	void powerUp(EmuTime::param time);
	void reset(EmuTime::param time);
	[[nodiscard]] byte readMem(byte address,EmuTime::param time);
	[[nodiscard]] byte peekMem(byte address,EmuTime::param time) const;
	void writeMem(byte address, byte value, EmuTime::param time);
	void setChipMode(ChipMode newMode);

	void serialize(Archive auto& ar, unsigned version);

private:
	// SoundDevice
	[[nodiscard]] float getAmplificationFactorImpl() const override;
	void generateChannels(float** bufs, unsigned num) override;

	[[nodiscard]] byte readWave(unsigned channel, unsigned address, EmuTime::param time) const;
	void writeWave(unsigned channel, unsigned address, byte value);
	void setDeformReg(byte value, EmuTime::param time);
	void setDeformRegHelper(byte value);
	void setFreqVol(unsigned address, byte value, EmuTime::param time);
	[[nodiscard]] byte getFreqVol(unsigned address) const;

private:
	static constexpr int CLOCK_FREQ = 3579545;

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debuggable;

	Clock<CLOCK_FREQ> deformTimer;
	ChipMode currentChipMode;

	signed char wave[5][32];
	float volAdjustedWave[5][32]; // ints stored as floats, see comment in adjust()
	unsigned incr[5];
	unsigned count[5];
	unsigned pos[5];
	unsigned period[5];
	unsigned orgPeriod[5];
	float out[5]; // ints stored as floats
	byte volume[5];
	byte ch_enable;

	byte deformValue;
	bool rotate[5];
	bool readOnly[5];
};

} // namespace openmsx

#endif
