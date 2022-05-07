/*
 * This class implements a simple timer, used in a TurboR.
 * This is a 16-bit (up) counter running at 255681Hz (3.58MHz / 14).
 * The least significant byte can be read from port 0xE6.
 *     most                                         0xE7.
 * Writing a random value to port 0xE6 resets the counter.
 * Writing to port 0xE7 has no effect.
 */

#ifndef MSXE6TIMER_HH
#define MSXE6TIMER_HH

#include "MSXDevice.hh"
#include "Clock.hh"

namespace openmsx {

class MSXE6Timer final : public MSXDevice
{
public:
	explicit MSXE6Timer(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	Clock<3579545, 14> reference; // (3.58 / 14)MHz
};

} // namespace openmsx

#endif
