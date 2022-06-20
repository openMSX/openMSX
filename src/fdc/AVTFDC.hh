#ifndef AVTFDC_HH
#define AVTFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class AVTFDC final : public WD2793BasedFDC
{
public:
	explicit AVTFDC(const DeviceConfig& config);

	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
