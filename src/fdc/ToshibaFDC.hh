#ifndef TOSHIBAFDC_HH
#define TOSHIBAFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class ToshibaFDC final : public WD2793BasedFDC
{
public:
	explicit ToshibaFDC(const DeviceConfig& config);

	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	bool r7ff5 = false; // only bit 0 contains storage
	bool r7ff6 = false; // "
};

} // namespace openmsx

#endif
