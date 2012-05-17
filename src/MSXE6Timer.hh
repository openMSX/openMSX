// $Id$

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

class MSXE6Timer : public MSXDevice
{
public:
	explicit MSXE6Timer(const DeviceConfig& config);

	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual void writeIO(word port, byte value, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Clock<3579545, 14> reference; // (3.58 / 14)Mhz
};

} // namespace openmsx

#endif
