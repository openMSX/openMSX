#ifndef ROMNATIONAL_HH
#define ROMNATIONAL_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomNational final : public Rom16kBBlocks
{
public:
	RomNational(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomNational();

	virtual void reset(EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	int sramAddr;
	byte control;
	byte bankSelect[4];
};

} // namespace openmsx

#endif
