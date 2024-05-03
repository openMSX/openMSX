#ifndef MSXBUNSETSU_HH
#define MSXBUNSETSU_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class MSXBunsetsu final : public MSXDevice
{
public:
	explicit MSXBunsetsu(const DeviceConfig& config);

	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom bunsetsuRom;
	Rom jisyoRom;
	unsigned jisyoAddress;
};

} // namespace openmsx

#endif
