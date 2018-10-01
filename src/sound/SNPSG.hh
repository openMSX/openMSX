#ifndef SNPSG_HH
#define SNPSG_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SN76489;

/** Device that puts the Texas Instruments SN76489 sound chip at
  * a fixed I/O address.
  */
class SNPSG final : public MSXDevice
{
public:
	SNPSG(const DeviceConfig& config);
	~SNPSG();

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::unique_ptr<SN76489> sn76489;
};

} // namespace openmsx

#endif
