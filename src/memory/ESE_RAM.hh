#ifndef ESE_RAM_HH
#define ESE_RAM_HH

#include "MSXDevice.hh"
#include "SRAM.hh"
#include "RomBlockDebuggable.hh"

namespace openmsx {

class ESE_RAM final : public MSXDevice
{
public:
	explicit ESE_RAM(const DeviceConfig& config);

	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] unsigned getSramSize() const;
	void setSRAM(unsigned region, byte block);

private:
	SRAM sram;
	RomBlockDebuggable romBlockDebug;

	bool isWriteable[4]; // which region is readonly?
	byte mapped[4]; // which block is mapped in this region?
	const byte blockMask;
};

} // namespace openmsx

#endif
