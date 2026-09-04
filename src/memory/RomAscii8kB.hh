#ifndef ROMASCII8KB_HH
#define ROMASCII8KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomAscii8kB : public Rom8kBBlocks
{
public:
	/** @param reversedRegs when true the four bank registers map to the
	  *   memory regions in the opposite order (see writeMem()).
	  */
	RomAscii8kB(const DeviceConfig& config, Rom&& rom,
	            bool reversedRegs = false);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

private:
	const bool reversedRegs;
};

} // namespace openmsx

#endif
