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
	explicit MSXToshibaTcx200x(const DeviceConfig& config);

	void reset(EmuTime::param time) override;

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) const override;
	[[nodiscard]] virtual byte peekMem(word address, EmuTime::param time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	bool sramEnabled() const;
	byte getSelectedSegment() const;
	Rom rs232Rom;
	Rom wordProcessorRom;
	SRAM sram;
	byte controlReg;
	BooleanSetting copyButtonPressed;

};

} // namespace openmsx

#endif
