#ifndef VICTORFDC_HH
#define VICTORFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class VictorFDC final : public WD2793BasedFDC
{
public:
	explicit VictorFDC(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word address) const override;
	bool allowUnaligned() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte driveControls;
};

} // namespace openmsx

#endif
