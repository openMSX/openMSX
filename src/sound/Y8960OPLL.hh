#ifndef Y8960OPLL_HH
#define Y8960OPLL_HH

#include "ResampledSoundDevice.hh"
#include "Y8960Pan.hh"
#include "YM2413NukeYKTBanked.hh"

#include "EmuTime.hh"
#include "SimpleDebuggable.hh"

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace openmsx {

/** The OPLL block of the Y8960: a YM2413 with a preset tone bank per
  * channel (40h-48h) and a pan per channel (50h-58h).
  *
  * The core handles everything up to 4Fh with the chip's timing. The pan
  * registers are taken here, in front of the core: they belong to the
  * mixing stage behind the chip, not to the chip. So this keeps a copy of
  * the address latch. The rhythm sounds take the pan of the channel they
  * are generated on: BD channel 7, HH and SD channel 8, TOM and TC
  * channel 9, counting from 1 as the chip's registers do.
  *
  * This is one stereo sound device rather than a mono one with a balance
  * because the pan is per channel, while openMSX only has a balance per
  * device.
  */
class Y8960OPLL final : public ResampledSoundDevice
{
public:
	static constexpr unsigned NUM_MELODY = 9;
	static constexpr unsigned NUM_RHYTHM = 5;
	static constexpr unsigned NUM_CHANNELS = NUM_MELODY + NUM_RHYTHM;
	static constexpr uint8_t PAN_BASE = 0x50;
	/** How much of the register space the debugger shows. */
	static constexpr unsigned REG_COUNT = 0x60;

	Y8960OPLL(const std::string& name, const DeviceConfig& config);
	~Y8960OPLL();

	void reset(EmuTime time);
	void writePort(bool port, uint8_t value, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	void setOutputRate(unsigned hostSampleRate, double speed) override;
	void generateChannels(std::span<float*> bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	void pokeReg(uint8_t reg, uint8_t value, EmuTime time);
	[[nodiscard]] uint8_t peekReg(uint8_t reg) const;
	void setPan(unsigned channel, uint8_t value);

private:
	YM2413NukeYKTBanked::YM2413 core;
	uint8_t latch;
	std::array<uint8_t, NUM_MELODY> pan;
	std::array<y8960::PanGain, NUM_MELODY> gain; // derived from pan

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} debuggable;
};

} // namespace openmsx

#endif
