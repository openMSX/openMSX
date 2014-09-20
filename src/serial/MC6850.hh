#ifndef MC6850_HH
#define MC6850_HH

#include "MSXDevice.hh"

namespace openmsx {

class MC6850 final : public MSXDevice
{
public:
	explicit MC6850(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
