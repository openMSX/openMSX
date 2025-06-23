#ifndef MSXBUNSETSU_HH
#define MSXBUNSETSU_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class MSXBunsetsu final : public MSXDevice
{
public:
	explicit MSXBunsetsu(DeviceConfig& config);

	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom bunsetsuRom;
	Rom jisyoRom;
	unsigned jisyoAddress;
};

} // namespace openmsx

#endif
