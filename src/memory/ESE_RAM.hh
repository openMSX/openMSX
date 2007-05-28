// $Id$

#ifndef ESE_RAM_HH
#define ESE_RAM_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SRAM;
class MSXCPU;

class ESE_RAM : public MSXDevice
{
public:
	ESE_RAM(MSXMotherBoard& motherBoard, const XMLElement& config,
	         const EmuTime& time);
	virtual ~ESE_RAM();

	virtual void reset(const EmuTime& time);

	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

private: 
	void setSRAM(unsigned region, byte block);

	std::auto_ptr<SRAM> sram;
	MSXCPU& cpu;

	bool isWriteable[4]; // which region is readonly?
	byte mapped[4]; // which block is mapped in this region?
	byte blockMask;
};

} // namespace openmsx

#endif
