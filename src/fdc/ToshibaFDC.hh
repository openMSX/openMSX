#ifndef TOSHIBAFDC_HH
#define TOSHIBAFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class ToshibaFDC final : public WD2793BasedFDC
{
public:
	explicit ToshibaFDC(DeviceConfig& config);

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
