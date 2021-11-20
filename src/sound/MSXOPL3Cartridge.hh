#ifndef MSXOPL3CARTRIDGE_HH
#define MSXOPL3CARTRIDGE_HH

#include "MSXDevice.hh"
#include "YMF262.hh"

namespace openmsx {

class MSXOPL3Cartridge final : public MSXDevice
{
public:
	explicit MSXOPL3Cartridge(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	YMF262 ymf262;
	int opl3latch;
};

} // namespace openmsx

#endif
