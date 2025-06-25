#ifndef MSXMEMORYMAPPERBASE_HH
#define MSXMEMORYMAPPERBASE_HH

#include "MSXDevice.hh"
#include "MSXMapperIO.hh"
#include "CheckedRam.hh"
#include "SimpleDebuggable.hh"
#include "narrow.hh"
#include <array>

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

	[[nodiscard]] unsigned getSizeInBlocks() const {
		return narrow<unsigned>(checkedRam.size() / 0x4000);
	}

	void reset(EmuTime time) override;
	void powerUp(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] unsigned getBaseSizeAlignment() const override;

	// Subclasses _must_ override this method and
	//  - call MSXMemoryMapperBase::writeIOImpl()
	//  - handle CPU cacheline stuff (e.g. invalidate)
	void writeIO(uint16_t port, byte value, EmuTime time) override = 0;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	/** Converts a Z80 address to a RAM address.
	  * @param address Index in Z80 address space.
	  * @return Index in RAM address space.
	  */
	[[nodiscard]] unsigned calcAddress(uint16_t address) const;
	[[nodiscard]] unsigned segmentOffset(byte page) const;

	void writeIOImpl(uint16_t port, byte value, EmuTime time);

	CheckedRam checkedRam;
	std::array<byte, 4> registers;

private:
	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
		void readBlock(unsigned start, std::span<byte> output) override;
	} debuggable;
};
SERIALIZE_CLASS_VERSION(MSXMemoryMapperBase, 2);

REGISTER_BASE_NAME_HELPER(MSXMemoryMapperBase, "MemoryMapper"); // keep old name for bw-compat

} // namespace openmsx

#endif
