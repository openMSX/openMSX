#ifndef MSXHIRESTIMER_HH
#define MSXHIRESTIMER_HH

#include "Clock.hh"
#include "MSXDevice.hh"

namespace openmsx {

class MSXHiResTimer final : public MSXDevice
{
public:
	explicit MSXHiResTimer(const DeviceConfig& config);

	void reset(EmuTime time) override;

	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Clock<3579545> reference; // last time the counter was reset
	unsigned latchedValue;    // last latched timer value
};

} // namespace openmsx

#endif
