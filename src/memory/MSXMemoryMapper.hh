// $Id$

#ifndef __MSXMEMORYMAPPER_HH__
#define __MSXMEMORYMAPPER_HH__

#include "MSXMemDevice.hh"
#include "Ram.hh"

namespace openmsx {

class Config;
class MSXMapperIO;

class MSXMemoryMapper : public MSXMemDevice
{
public:
	MSXMemoryMapper(Config* config, const EmuTime& time);
	virtual ~MSXMemoryMapper();
	
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;
	
	virtual void reset(const EmuTime& time);

protected:
	Ram* ram;

private:
	void createMapperIO(const EmuTime& time);
	void destroyMapperIO();

	/** Converts a Z80 address to a RAM address.
	  * @param address Index in Z80 address space.
	  * @return Index in RAM address space.
	  */
	inline unsigned calcAddress(word address) const;

	unsigned nbBlocks;
	bool slowDrainOnReset;

	static unsigned counter;
	static Config* device;
	static MSXMapperIO* mapperIO;
};

} // namespace openmsx

#endif //__MSXMEMORYMAPPER_HH__
