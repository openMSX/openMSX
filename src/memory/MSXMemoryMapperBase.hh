#ifndef MSXMEMORYMAPPERBASE_HH
#define MSXMEMORYMAPPERBASE_HH

#include "MSXDevice.hh"
#include "MSXMapperIO.hh"
#include "CheckedRam.hh"
#include "SimpleDebuggable.hh"

namespace openmsx {

class MSXMemoryMapperBase : public MSXDevice, public MSXMapperIOClient
{
public:
	explicit MSXMemoryMapperBase(const DeviceConfig& config);

	/**
	 * Returns the currently selected segment for the given page.
	 * @param page Z80 address page (0-3).
	 */
	[[nodiscard]] byte getSelectedSegment(byte page) const override { return registers[page]; }

	[[nodiscard]] unsigned getSizeInBlocks() { return checkedRam.getSize() / 0x4000; }

	void reset(EmuTime::param time) override;
	void powerUp(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) const override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] unsigned getBaseSizeAlignment() const override;

	// Subclasses _must_ override this method and
	//  - call MSXMemoryMapperBase::writeIOImpl()
	//  - handle CPU cacheline stuff (e.g. invalidate)
	void writeIO(word port, byte value, EmuTime::param time) override = 0;

	void serialize(Archive auto& ar, unsigned version);

protected:
	/** Converts a Z80 address to a RAM address.
	  * @param address Index in Z80 address space.
	  * @return Index in RAM address space.
	  */
	[[nodiscard]] unsigned calcAddress(word address) const;
	[[nodiscard]] unsigned segmentOffset(byte page) const;

	void writeIOImpl(word port, byte value, EmuTime::param time);

	CheckedRam checkedRam;
	byte registers[4];

private:
	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
	} debuggable;
};
SERIALIZE_CLASS_VERSION(MSXMemoryMapperBase, 2);

REGISTER_BASE_NAME_HELPER(MSXMemoryMapperBase, "MemoryMapper"); // keep old name for bw-compat

} // namespace openmsx

#endif
