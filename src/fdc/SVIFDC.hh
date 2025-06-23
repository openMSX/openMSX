#ifndef SVIFDC_HH
#define SVIFDC_HH

#include "WD2793BasedFDC.hh"

namespace openmsx {

class SVIFDC final : public WD2793BasedFDC
{
public:
	explicit SVIFDC(DeviceConfig& config);

	[[nodiscard]] byte readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime::param time) const override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
