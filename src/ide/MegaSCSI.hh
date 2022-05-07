#ifndef MEGASCSI_HH
#define MEGASCSI_HH

#include "MSXDevice.hh"
#include "MB89352.hh"
#include "SRAM.hh"
#include "RomBlockDebuggable.hh"

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

	void serialize(Archive auto& ar, unsigned version);

private:
	[[nodiscard]]unsigned getSramSize() const;
	void setSRAM(unsigned region, byte block);

private:
	MB89352 mb89352;
	SRAM sram;
	RomBlockDebuggable romBlockDebug;

	bool isWriteable[4]; // which region is readonly?
	byte mapped[4]; // SPC block mapped in this region?
	const byte blockMask;
};

} // namespace openmsx

#endif
