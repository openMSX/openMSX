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

	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	unsigned getSramSize() const;
	void setSRAM(unsigned region, byte block);

	MB89352 mb89352;
	SRAM sram;
	RomBlockDebuggable romBlockDebug;

	bool isWriteable[4]; // which region is readonly?
	byte mapped[4]; // SPC block mapped in this region?
	const byte blockMask;
};

} // namespace openmsx

#endif
