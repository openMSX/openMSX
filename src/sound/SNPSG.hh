#ifndef SNPSG_HH
#define SNPSG_HH

#include "MSXDevice.hh"
#include "SN76489.hh"

namespace openmsx {

/** Device that puts the Texas Instruments SN76489 sound chip at
  * a fixed I/O address.
  */
class SNPSG final : public MSXDevice
{
public:
	explicit SNPSG(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	SN76489 sn76489;
};

} // namespace openmsx

#endif
