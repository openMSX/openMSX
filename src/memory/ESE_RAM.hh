// $Id$

#ifndef ESE_RAM_HH
#define ESE_RAM_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SRAM;
class RomBlockDebuggable;

class ESE_RAM : public MSXDevice
{
public:
	ESE_RAM(const DeviceConfig& config);
	virtual ~ESE_RAM();

	virtual void reset(EmuTime::param time);

	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setSRAM(unsigned region, byte block);

	const std::unique_ptr<SRAM> sram;
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;

	bool isWriteable[4]; // which region is readonly?
	byte mapped[4]; // which block is mapped in this region?
	const byte blockMask;
};

} // namespace openmsx

#endif
