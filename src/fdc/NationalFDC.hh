#ifndef NATIONALFDC_HH
#define NATIONALFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class NationalFDC final : public WD2793BasedFDC
{
public:
	explicit NationalFDC(const DeviceConfig& config);

	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
