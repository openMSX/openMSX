// $Id$

#ifndef __MSXMEMORYMAPPER_HH__
#define __MSXMEMORYMAPPER_HH__

#include <memory>
#include "MSXDevice.hh"

namespace openmsx {

class XMLElement;
class MSXMapperIO;
class Ram;

class MSXMemoryMapper : public MSXDevice
{
public:
	MSXMemoryMapper(const XMLElement& config, const EmuTime& time);
	virtual ~MSXMemoryMapper();
	
	virtual void powerUp(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;
	
	virtual void reset(const EmuTime& time);

protected:
	std::auto_ptr<Ram> ram;

private:
	void createMapperIO(const EmuTime& time);
	void destroyMapperIO();

	/** Converts a Z80 address to a RAM address.
	  * @param address Index in Z80 address space.
	  * @return Index in RAM address space.
	  */
	inline unsigned calcAddress(word address) const;

	unsigned nbBlocks;

	static unsigned counter;
	static XMLElement* config;
	static MSXMapperIO* mapperIO;
};

} // namespace openmsx

#endif //__MSXMEMORYMAPPER_HH__
