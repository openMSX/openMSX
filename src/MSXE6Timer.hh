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

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "emutime.hh"

class MSXE6Timer : public MSXDevice
{
	public:
		MSXE6Timer();
		~MSXE6Timer();
		
		void init();
		void reset();
		
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);

	private:
		Emutime reference;
};

#endif //__E6TIMER_HH__
