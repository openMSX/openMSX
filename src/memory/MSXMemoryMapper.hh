// $Id$

#ifndef __MSXMEMORYMAPPER_HH__
#define __MSXMEMORYMAPPER_HH__

#include "MSXMemDevice.hh"
#include "Debuggable.hh"

namespace openmsx {

class Device;
class MSXMapperIO;

class MSXMemoryMapper : public MSXMemDevice, private Debuggable
{
public:
	MSXMemoryMapper(Device* config, const EmuTime& time);
	virtual ~MSXMemoryMapper();
	
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;
	
	virtual void reset(const EmuTime& time);

protected:
	void createMapperIO(const EmuTime& time);
	void destroyMapperIO();

	/** Converts a Z80 address to a RAM address.
	  * @param address Index in Z80 address space.
	  * @return Index in RAM address space.
	  */
	inline unsigned calcAddress(word address) const;

	byte* buffer;
	unsigned nbBlocks;
	bool slowDrainOnReset;

	static unsigned counter;
	static Device* device;
	static MSXMapperIO* mapperIO;

private:
	// Debuggable
	virtual unsigned getSize() const;
	virtual const string& getDescription() const;
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
};

} // namespace openmsx

#endif //__MSXMEMORYMAPPER_HH__
