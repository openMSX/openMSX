#ifndef GOUDASCSI_HH
#define GOUDASCSI_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include "WD33C93.hh"

namespace openmsx {

class GoudaSCSI final : public MSXDevice
{
public:
	explicit GoudaSCSI(DeviceConfig& config);

	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom rom;
	WD33C93 wd33c93;
};

} // namespace openmsx

#endif
