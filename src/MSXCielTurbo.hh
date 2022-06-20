#ifndef MSXCIELTURBO_HH
#define MSXCIELTURBO_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXCielTurbo final : public MSXDevice
{
public:
	explicit MSXCielTurbo(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte lastValue;
};

} // namespace openmsx

#endif
