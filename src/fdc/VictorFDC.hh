#ifndef VICTORFDC_HH
#define VICTORFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class VictorFDC final : public WD2793BasedFDC
{
public:
	explicit VictorFDC(DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;
	[[nodiscard]] bool allowUnaligned() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte driveControls;
};

} // namespace openmsx

#endif
