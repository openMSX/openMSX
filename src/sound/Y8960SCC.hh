#ifndef Y8960SCC_HH
#define Y8960SCC_HH

#include "ResampledSoundDevice.hh"
#include "Y8960Pan.hh"

#include "Clock.hh"
#include "SimpleDebuggable.hh"
#include "serialize_meta.hh"

#include <array>
#include <cstdint>

namespace openmsx {

/** The SCC of the Y8960: a Konami SCC whose five channels each have a pan
  * register.
  *
  * This is a copy of SCC.cc, the openMSX SCC, cut down to the standard SCC
  * (Mode::Real) and made a stereo device. The pan is per channel, while
  * openMSX only has a balance per device, so the channels have to be spread
  * here; SCC is final and keeps its generator private, so it is copied
  * rather than wrapped.
  *
  * The pan registers do not sit in the 256 bytes the SCC decodes. The
  * mapper takes them and calls setPan().
  */
class Y8960SCC final : public ResampledSoundDevice
{
public:
	static constexpr unsigned NUM_CHANNELS = 5;

	Y8960SCC(const std::string& name, const DeviceConfig& config, EmuTime time);
	~Y8960SCC();

	void powerUp(EmuTime time);
	void reset(EmuTime time);
	[[nodiscard]] uint8_t readMem(uint8_t address, EmuTime time);
	[[nodiscard]] uint8_t peekMem(uint8_t address, EmuTime time) const;
	void writeMem(uint8_t address, uint8_t value, EmuTime time);
	void setPan(unsigned channel, uint8_t value, EmuTime time);

	[[nodiscard]] const std::array<std::array<int8_t, 32>, 5>& getWaveData() const { return wave; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	[[nodiscard]] float getAmplificationFactorImpl() const override;
	void generateChannels(std::span<float*> bufs, unsigned num) override;

	[[nodiscard]] uint8_t readWave(unsigned channel, unsigned address, EmuTime time) const;
	void writeWave(unsigned channel, unsigned address, uint8_t value);
	void setDeformReg(uint8_t value, EmuTime time);
	void setDeformRegHelper(uint8_t value);
	void setFreqVol(unsigned address, uint8_t value, EmuTime time);
	[[nodiscard]] uint8_t getFreqVol(unsigned address) const;
	void storePan(unsigned channel, uint8_t value);

private:
	static constexpr int CLOCK_FREQ = 3579545;

	/** Same name and layout as the SCC's, so the debugger sees this one as
	  * it saw the plain SCC here before. */
	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} debuggable;

	struct PanDebuggable final : SimpleDebuggable {
		PanDebuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} panDebuggable;

	Clock<CLOCK_FREQ> deformTimer;

	std::array<std::array<int8_t, 32>, 5> wave;
	std::array<std::array<float, 32>, 5> volAdjustedWave; // ints stored as floats, see comment in adjust()
	std::array<unsigned, 5> incr;
	std::array<unsigned, 5> count;
	std::array<unsigned, 5> pos;
	std::array<unsigned, 5> period;
	std::array<unsigned, 5> orgPeriod;
	std::array<float, 5> out; // ints stored as floats
	std::array<uint8_t, 5> volume;
	uint8_t ch_enable;

	uint8_t deformValue;
	std::array<bool, 5> rotate;
	std::array<bool, 5> readOnly;

	std::array<uint8_t, NUM_CHANNELS> pan;
	std::array<y8960::PanGain, NUM_CHANNELS> gain; // derived from pan
};
SERIALIZE_CLASS_VERSION(Y8960SCC, 2);

} // namespace openmsx

#endif
