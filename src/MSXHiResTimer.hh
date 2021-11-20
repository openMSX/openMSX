#ifndef MSXHIRESTIMER_HH
#define MSXHIRESTIMER_HH

#include "MSXDevice.hh"
#include "Clock.hh"

namespace openmsx {

class MSXHiResTimer final : public MSXDevice
{
public:
	explicit MSXHiResTimer(const DeviceConfig& config);

	void reset(EmuTime::param time) override;

	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Clock<3579545> reference; // last time the counter was reset
	unsigned latchedValue;    // last latched timer value
};

} // namespace openmsx

#endif
