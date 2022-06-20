#ifndef NATIONALFDC_HH
#define NATIONALFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class NationalFDC final : public WD2793BasedFDC
{
public:
	explicit NationalFDC(const DeviceConfig& config);

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
