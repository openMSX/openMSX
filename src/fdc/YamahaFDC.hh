#ifndef YAMAHAFDC_HH
#define YAMAHAFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class YamahaFDC final : public WD2793BasedFDC
{
public:
	explicit YamahaFDC(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
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
