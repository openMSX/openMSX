#ifndef MSXTOSHIBATCX200X_HH
#define MSXTOSHIBATCX200X_HH

#include "MSXDevice.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "BooleanSetting.hh"

namespace openmsx {

class MSXToshibaTcx200x final : public MSXDevice
{
public:
	explicit MSXToshibaTcx200x(DeviceConfig& config);

	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool sramEnabled() const;
	[[nodiscard]] byte getSelectedSegment() const;

private:
	Rom rs232Rom;
	Rom wordProcessorRom;
	SRAM sram;
	byte controlReg;
	BooleanSetting copyButtonPressed;
};

} // namespace openmsx

#endif
