#ifndef MSXMAKOTO_HH
#define MSXMAKOTO_HH

#include "YM2608.hh"

#include "MSXDevice.hh"

namespace openmsx {

class MSXMakoto final : public MSXDevice
{
public:
	explicit MSXMakoto(DeviceConfig& config);

	void reset(EmuTime time) override;
	uint8_t readIO(uint16_t port, EmuTime time) override;
	uint8_t peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	YM2608 ym2608;
};

} // namespace openmsx

#endif
