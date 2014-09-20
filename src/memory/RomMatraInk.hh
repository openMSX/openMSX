#ifndef ROMMATRAINK_HH
#define ROMMATRAINK_HH

#include "MSXRom.hh"
#include <memory>

namespace openmsx {

class Rom;
class AmdFlash;

class RomMatraInk final : public MSXRom
{
public:
	RomMatraInk(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomMatraInk();

	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<AmdFlash> flash;
};

} // namespace openmsx

#endif
