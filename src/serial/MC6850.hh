#ifndef MC6850_HH
#define MC6850_HH

#include "MSXDevice.hh"

namespace openmsx {

class MC6850 final : public MSXDevice
{
public:
	explicit MC6850(const DeviceConfig& config);

	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
