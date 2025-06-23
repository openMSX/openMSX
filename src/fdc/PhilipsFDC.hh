#ifndef PHILIPSFDC_HH
#define PHILIPSFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class PhilipsFDC final : public WD2793BasedFDC
{
public:
	explicit PhilipsFDC(DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte sideReg;
	byte driveReg;
};

} // namespace openmsx

#endif
