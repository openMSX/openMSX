#ifndef ROMDOOLY_HH
#define ROMDOOLY_HH

#include "MSXRom.hh"

namespace openmsx {

class RomBlockDebuggable;

class RomDooly final : public MSXRom
{
public:
	RomDooly(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomDooly();

	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;
	byte conversion;
};

} // namspace openmsx

#endif
