#ifndef ESE_RAM_HH
#define ESE_RAM_HH

#include "MSXDevice.hh"
#include "SRAM.hh"
#include "RomBlockDebuggable.hh"
#include <array>

namespace openmsx {

class ESE_RAM final : public MSXDevice
{
public:
	explicit ESE_RAM(const DeviceConfig& config);

	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] size_t getSramSize() const;
	void setSRAM(unsigned region, byte block);

private:
	SRAM sram;
	RomBlockDebuggable romBlockDebug;

	std::array<bool, 4> isWriteable; // which region is readonly?
	std::array<byte, 4> mapped; // which block is mapped in this region?
	const byte blockMask;
};

} // namespace openmsx

#endif
