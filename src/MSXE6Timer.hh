// $Id$

/*
 * This class implements a simple timer, used in a TurboR.
 * This is a 16-bit (up) counter running at 255681Hz (3.58MHz / 14).
 * The least significant byte can be read from port 0xE6.
 *     most                                         0xE7.
 * Writing a random value to port 0xE6 resets the counter.
 * Writing to port 0xE7 has no effect.
 */

#ifndef __E6TIMER_HH__
#define __E6TIMER_HH__

#include "MSXIODevice.hh"
#include "EmuTime.hh"


namespace openmsx {

class MSXE6Timer : public MSXIODevice
{
	public:
		MSXE6Timer(Device *config, const EmuTime &time);
		virtual ~MSXE6Timer();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);

	private:
		EmuTimeFreq<255681> reference;	// 1/14 * 3.58MHz
};

} // namespace openmsx

#endif //__E6TIMER_HH__
