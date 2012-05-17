// $Id$

#ifndef MSXMEMORYMAPPER_HH
#define MSXMEMORYMAPPER_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class MSXMapperIO;
class CheckedRam;

class MSXMemoryMapper : public MSXDevice
{
public:
	explicit MSXMemoryMapper(const DeviceConfig& config);
	virtual ~MSXMemoryMapper();

	virtual void reset(EmuTime::param time);
	virtual void powerUp(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;
	virtual byte peekMem(word address, EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	/** Converts a Z80 address to a RAM address.
	  * @param address Index in Z80 address space.
	  * @return Index in RAM address space.
	  */
	unsigned calcAddress(word address) const;

	const std::auto_ptr<CheckedRam> checkedRam;

private:
	MSXMapperIO& mapperIO;
};

REGISTER_BASE_NAME_HELPER(MSXMemoryMapper, "MemoryMapper");

} // namespace openmsx

#endif
