#ifndef MEGASCSI_HH
#define MEGASCSI_HH

#include "MSXDevice.hh"
#include "MB89352.hh"
#include "SRAM.hh"
#include "RomBlockDebuggable.hh"
#include <array>

namespace openmsx {

class MegaSCSI final : public MSXDevice
{
public:
	explicit MegaSCSI(const DeviceConfig& config);

	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]]byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]]const byte* getReadCacheLine(word address) const override;
	[[nodiscard]]byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] size_t getSramSize() const;
	void setSRAM(unsigned region, byte block);

private:
	MB89352 mb89352;
	SRAM sram;
	RomBlockDebuggable romBlockDebug;

	std::array<bool, 4> isWriteable; // which region is readonly?
	std::array<byte, 4> mapped; // SPC block mapped in this region?
	const byte blockMask;
};

} // namespace openmsx

#endif
