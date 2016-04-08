#ifndef SPECTRAVIDEOFDC_HH
#define SPECTRAVIDEOFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class SpectravideoFDC final : public WD2793BasedFDC
{
public:
	explicit SpectravideoFDC(const DeviceConfig& config);

	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	bool cpmRomEnabled;
};

} // namespace openmsx

#endif
