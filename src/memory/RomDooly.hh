#ifndef ROMDOOLY_HH
#define ROMDOOLY_HH

#include "MSXRom.hh"

namespace openmsx {

class RomBlockDebuggable;

class RomDooly : public MSXRom
{
public:
	RomDooly(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	virtual ~RomDooly();

	virtual void reset(EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;
	byte conversion;
};

} // namspace openmsx

#endif
