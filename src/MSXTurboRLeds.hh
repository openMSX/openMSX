// $Id$

/*
 * This class implements the 2 Turbo-R specific LEDS:
 *  
 * Bit 0 of IO-port 0xA7 turns the PAUSE led ON (1) or OFF (0) 
 * Bit 7                           TURBO
 * This port can only be written to.
 */

#ifndef __TURBORLEDS_HH__
#define __TURBORLEDS_HH__

#include "MSXIODevice.hh"


class MSXTurboRLeds : public MSXIODevice
{
	public:
		MSXTurboRLeds(Device *config, const EmuTime &time);
		virtual ~MSXTurboRLeds();
		
		virtual void reset(const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
};

#endif
