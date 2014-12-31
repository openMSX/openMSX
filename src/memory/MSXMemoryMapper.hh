#ifndef MSXMEMORYMAPPER_HH
#define MSXMEMORYMAPPER_HH

#include "MSXDevice.hh"
#include "CheckedRam.hh"

namespace openmsx {

class MSXMapperIO;

class MSXMemoryMapper : public MSXDevice
{
public:
	explicit MSXMemoryMapper(const DeviceConfig& config);
	virtual ~MSXMemoryMapper();

	void reset(EmuTime::param time) override;
	void powerUp(EmuTime::param time) override;
	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word start) const override;
	byte peekMem(word address, EmuTime::param time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	/** Converts a Z80 address to a RAM address.
	  * @param address Index in Z80 address space.
	  * @return Index in RAM address space.
	  */
	unsigned calcAddress(word address) const;

	CheckedRam checkedRam;

private:
	unsigned getRamSize() const;

	MSXMapperIO& mapperIO;
};

REGISTER_BASE_NAME_HELPER(MSXMemoryMapper, "MemoryMapper");

} // namespace openmsx

#endif
