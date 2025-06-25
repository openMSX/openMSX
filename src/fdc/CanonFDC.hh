#ifndef CANONFDC_HH
#define CANONFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class CanonFDC final : public WD2793BasedFDC
{
public:
	explicit CanonFDC(DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte controlReg;
};

} // namespace openmsx

#endif
