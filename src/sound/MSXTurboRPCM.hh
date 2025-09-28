#ifndef MSXTURBORPCM_HH
#define MSXTURBORPCM_HH

#include "AudioInputConnector.hh"
#include "DACSound8U.hh"

#include "Clock.hh"
#include "MSXDevice.hh"

namespace openmsx {

class MSXMixer;

class MSXTurboRPCM final : public MSXDevice
{
public:
	explicit MSXTurboRPCM(const DeviceConfig& config);
	~MSXTurboRPCM() override;

	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] byte getSample(EmuTime time) const;
	[[nodiscard]] bool getComp(EmuTime time) const;
	void hardwareMute(bool mute);

private:
	MSXMixer& mixer;
	AudioInputConnector connector;
	DACSound8U dac;
	Clock<3579545, 228> reference; // 15700Hz
	byte DValue;
	byte status;
	byte hold;
	bool hwMute = false;
};

} // namespace openmsx

#endif
