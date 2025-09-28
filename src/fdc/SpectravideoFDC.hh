#ifndef SPECTRAVIDEOFDC_HH
#define SPECTRAVIDEOFDC_HH

#include "WD2793BasedFDC.hh"

#include "Rom.hh"

namespace openmsx {

class SpectravideoFDC final : public WD2793BasedFDC
{
public:
	explicit SpectravideoFDC(DeviceConfig& config);

	void reset(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	bool cpmRomEnabled;
	Rom cpmRom;
};

} // namespace openmsx

#endif
