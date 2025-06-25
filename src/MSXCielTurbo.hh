#ifndef MSXCIELTURBO_HH
#define MSXCIELTURBO_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXCielTurbo final : public MSXDevice
{
public:
	explicit MSXCielTurbo(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte lastValue;
};

} // namespace openmsx

#endif
