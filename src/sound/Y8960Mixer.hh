#ifndef Y8960MIXER_HH
#define Y8960MIXER_HH

#include "EnumSetting.hh"
#include "MSXDevice.hh"
#include "serialize_meta.hh"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace openmsx {

/** The Y8960's sound output and its digital mixer.
  *
  * The cartridge does **not** feed its audio back into the MSX; the two
  * outputs are separate wires and whoever owns the machine either switches
  * between them or mixes them. That choice is the output setting below.
  *
  * The mixer proper is one volume per sound block, written through the
  * memory mapped I/O at 7FF7h + channel, and a master volume at 7FFEh that
  * scales all of them. It has no I/O ports. The ADPCM has no volume here: it
  * is mixed into the OPL2 and takes the OPL2's.
  */
class Y8960Mixer final : public MSXDevice, private Observer<Setting>
{
public:
	// Channels 4 and 5 (7FFBh / 7FFCh) are the OPM's address and data
	// ports, not volumes; the channel number stays the offset from 7FF7h,
	// so they are counted here but never wired to a device.
	static constexpr int ChannelCount = 7;
	static constexpr int OldChannelCount = 4; // savestates before version 5

	/** Which of the two outputs is being listened to. The cartridge does
	  * not feed its audio back into the MSX, so on real hardware this is
	  * a choice someone makes outside the machine. */
	enum class Output : uint8_t { Msx, Y8960, Mix };

	explicit Y8960Mixer(const DeviceConfig& config);
	~Y8960Mixer() override;

	void reset(EmuTime time) override;

	void writeVolume(int channel, uint8_t value);
	void writeMasterVolume(uint8_t value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] std::optional<int> findChannel(std::string_view name) const;
	void applyAllGains();
	void update(const Setting& setting) noexcept override;

private:
	std::array<std::vector<std::string_view>, ChannelCount> channelDevices;
	std::array<uint8_t, ChannelCount> volume;
	uint8_t masterVolume;
	std::unique_ptr<EnumSetting<Output>> outputSetting;
};
SERIALIZE_CLASS_VERSION(Y8960Mixer, 5);

} // namespace openmsx

#endif
