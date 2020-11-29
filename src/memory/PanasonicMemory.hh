#ifndef PANASONICMEMORY_HH
#define PANASONICMEMORY_HH

#include "openmsx.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class MSXCPU;
class Ram;
class Rom;

class PanasonicMemory
{
public:
	explicit PanasonicMemory(MSXMotherBoard& motherBoard);
	~PanasonicMemory();

	/**
	 * Pass reference of the actual Ram block for use in DRAM mode and RAM
	 * access via the ROM mapper. Note that this is always unchecked Ram!
	 */
	void registerRam(Ram& ram);
	[[nodiscard]] const byte* getRomBlock(unsigned block);
	[[nodiscard]] const byte* getRomRange(unsigned first, unsigned last);
	/**
	 * Note that this is always unchecked RAM! There is no UMR detection
	 * when accessing Ram in DRAM mode or via the ROM mapper!
	 */
	[[nodiscard]] byte* getRamBlock(unsigned block);
	[[nodiscard]] unsigned getRamSize() const { return ramSize; }
	void setDRAM(bool dram);
	[[nodiscard]] bool isWritable(unsigned address) const;

private:
	MSXCPU& msxcpu;

	const std::unique_ptr<Rom> rom; // can be nullptr
	byte* ram;
	unsigned ramSize;
	bool dram;
};

} // namespace openmsx

#endif
