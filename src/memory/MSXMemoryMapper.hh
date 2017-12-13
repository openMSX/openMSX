#ifndef MSXMEMORYMAPPER_HH
#define MSXMEMORYMAPPER_HH

#include "MSXDevice.hh"
#include "CheckedRam.hh"
#include "SimpleDebuggable.hh"

namespace openmsx {

class MSXMapperIO;

class MSXMemoryMapper : public MSXDevice
{
public:
	explicit MSXMemoryMapper(const DeviceConfig& config);
	virtual ~MSXMemoryMapper();

	unsigned getSizeInBlocks() { return checkedRam.getSize() / 0x4000; }

	void reset(EmuTime::param time) override;
	void powerUp(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;
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

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
	} debuggable;

	MSXMapperIO& mapperIO;
	byte registers[4];
};
SERIALIZE_CLASS_VERSION(MSXMemoryMapper, 2);

REGISTER_BASE_NAME_HELPER(MSXMemoryMapper, "MemoryMapper");

} // namespace openmsx

#endif
