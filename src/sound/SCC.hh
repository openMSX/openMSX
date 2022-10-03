#ifndef SCC_HH
#define SCC_HH

#include "ResampledSoundDevice.hh"
#include "SimpleDebuggable.hh"
#include "Clock.hh"
#include "openmsx.hh"
#include <array>

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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	[[nodiscard]] float getAmplificationFactorImpl() const override;
	void generateChannels(std::span<float*> bufs, unsigned num) override;

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

	std::array<std::array<signed char, 32>, 5> wave;
	std::array<std::array<float, 32>, 5> volAdjustedWave; // ints stored as floats, see comment in adjust()
	std::array<unsigned, 5> incr;
	std::array<unsigned, 5> count;
	std::array<unsigned, 5> pos;
	std::array<unsigned, 5> period;
	std::array<unsigned, 5> orgPeriod;
	std::array<float, 5> out; // ints stored as floats
	std::array<byte, 5> volume;
	byte ch_enable;

	byte deformValue;
	std::array<bool, 5> rotate;
	std::array<bool, 5> readOnly;
};

} // namespace openmsx

#endif
