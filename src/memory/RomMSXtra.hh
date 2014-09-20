#ifndef ROMMSXTRA_HH
#define ROMMSXTRA_HH

#include "MSXRom.hh"

namespace openmsx {

class Ram;

class RomMSXtra final : public MSXRom
{
public:
	RomMSXtra(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomMSXtra();

	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<Ram> ram;
};

} // namespace openmsx

#endif
