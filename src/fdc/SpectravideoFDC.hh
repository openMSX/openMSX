#ifndef SPECTRAVIDEOFDC_HH
#define SPECTRAVIDEOFDC_HH

#include "WD2793BasedFDC.hh"
#include "Rom.hh"

namespace openmsx {

class SpectravideoFDC final : public WD2793BasedFDC
{
public:
	explicit SpectravideoFDC(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	bool cpmRomEnabled;
	Rom cpmRom;
};

} // namespace openmsx

#endif
