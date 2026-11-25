#ifndef SONYHBIF900_HH
#define SONYHBIF900_HH

#include "MSXDevice.hh"

namespace openmsx {

/** Control latch of the HBI-F900 AV Creator attached to an HB-F900.
  * The HB-F900 Video Utility ROM probes bit 7 of port F7 at boot and writes
  * this port for its AUDIO, VIDEOIN, GENLOCK and BRIGHT commands. The ROM
  * itself programs the V9938 for COMPUTER, SUPER, VIDEO and digitization.
  */
class SonyHBIF900 final : public MSXDevice
{
public:
	explicit SonyHBIF900(const DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void applyAudioRouting(EmuTime time) const;

	byte portF7 = 0xFF;
};

} // namespace openmsx

#endif
