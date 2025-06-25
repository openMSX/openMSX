#ifndef JVCMSXMIDI_HH
#define JVCMSXMIDI_HH

#include "MSXDevice.hh"
#include "MC6850.hh"

namespace openmsx {

class JVCMSXMIDI final : public MSXDevice
{
public:
	explicit JVCMSXMIDI(const DeviceConfig& config);

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

} // namespace openmsx

#endif
