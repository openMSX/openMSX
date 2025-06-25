#ifndef MSXOPL3CARTRIDGE_HH
#define MSXOPL3CARTRIDGE_HH

#include "MSXDevice.hh"
#include "YMF262.hh"

namespace openmsx {

class MSXOPL3Cartridge final : public MSXDevice
{
public:
	explicit MSXOPL3Cartridge(const DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	YMF262 ymf262;
	int opl3latch;
};

} // namespace openmsx

#endif
