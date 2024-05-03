#ifndef TOSHIBAFDC_HH
#define TOSHIBAFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class ToshibaFDC final : public WD2793BasedFDC
{
public:
	explicit ToshibaFDC(const DeviceConfig& config);

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
