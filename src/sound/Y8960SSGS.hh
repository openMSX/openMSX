#ifndef Y8960SSGS_HH
#define Y8960SSGS_HH

#include "ResampledSoundDevice.hh"
#include "Y8960Pan.hh"
#include "Y8960SsgCore.hh"

#include "EmuTime.hh"
#include "SimpleDebuggable.hh"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace openmsx {

/** The SSGS block of the Y8960: two YM2149 units, each channel with its own
  * volume and pan.
  *
  * Register map (the `ssg` sheet of the specification):
  *
  *   00h-0Fh  unit 0, YM2149 registers
  *   10h-1Fh  unit 1, YM2149 registers
  *   20h-22h  volume of unit 0 channel A, B, C   (0 mutes, 128 is unity)
  *   24h-26h  volume of unit 1 channel A, B, C
  *   28h-2Ah  pan of unit 0 channel A, B, C      (1 left, 128 centre, 255 right)
  *   2Ch-2Eh  pan of unit 1 channel A, B, C
  *
  * 23h, 27h, 2Bh and 2Fh are not connected. 0Eh/0Fh and 1Eh/1Fh are the
  * units' GPIO registers, kept as plain storage. On the cartridge the second
  * unit's port A reads a DIP switch, but no read reaches it here because
  * the device answers no reads (see Y8960SSGSDevice).
  *
  * This is one stereo sound device rather than two mono ones because the
  * pan is per channel, while openMSX only has a balance per device.
  */
class Y8960SSGS final : public ResampledSoundDevice
{
public:
	static constexpr unsigned NUM_UNITS = 2;
	static constexpr unsigned CHANNELS_PER_UNIT = 3;
	static constexpr unsigned NUM_CHANNELS = NUM_UNITS * CHANNELS_PER_UNIT;

	static constexpr unsigned UNIT_REGS = 0x10;
	static constexpr unsigned VOLUME_BASE = 0x20;
	static constexpr unsigned PAN_BASE = 0x28;
	static constexpr unsigned REG_END = 0x30;

	Y8960SSGS(const std::string& name, const DeviceConfig& config, EmuTime time);
	~Y8960SSGS();

	void reset(EmuTime time);
	void writeRegister(unsigned reg, uint8_t value, EmuTime time);
	[[nodiscard]] uint8_t peekRegister(unsigned reg, EmuTime time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	void generateChannels(std::span<float*> bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	void measureDc();
	void updateGain(unsigned channel);
	/** The channel a volume or pan register (20h-2Fh) addresses, or
	  * nothing for the four unconnected slots. */
	[[nodiscard]] static std::optional<unsigned> mixChannel(unsigned reg);

private:
	std::array<Y8960SsgCore, NUM_UNITS> unit;
	std::array<uint8_t, NUM_CHANNELS> volume;
	std::array<uint8_t, NUM_CHANNELS> pan;
	std::array<y8960::PanGain, NUM_CHANNELS> gain; // derived from volume and pan

	/** The core's output is unipolar, so a silent channel does not sit at
	  * zero. That offset has to come off before the pan is applied,
	  * otherwise the DC leaks into the stereo image. */
	std::array<float, NUM_CHANNELS> dc;

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} debuggable;
};

} // namespace openmsx

#endif
