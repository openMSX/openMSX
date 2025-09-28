#ifndef MUSICMODULEMIDI_HH
#define MUSICMODULEMIDI_HH

#include "MC6850.hh"
#include "MSXDevice.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MusicModuleMIDI final : public MSXDevice
{
public:
	explicit MusicModuleMIDI(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime time) override;
	[[nodiscard]] uint8_t readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MC6850 mc6850;
};
SERIALIZE_CLASS_VERSION(MusicModuleMIDI, 4);

} // namespace openmsx

#endif
