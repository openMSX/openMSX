// $Id$

/*
 * This class implements the MSX Turbo-R pause key
 * 
 *  Whenever the pause key is pressed a flip-flop is toggled.
 *  The status of this flip-flop can be read from io-port 0xA7.
 *   bit 0 indicates the status (1 = pause active)
 *   all other bits read 0
 */

#ifndef __TURBORPAUSE_HH__
#define __TURBORPAUSE_HH__

#include "MSXIODevice.hh"


namespace openmsx {

class MSXTurboRPause : public MSXIODevice
{
	public:
		MSXTurboRPause(Device *config, const EmuTime &time);
		virtual ~MSXTurboRPause();
		
		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
	
	private:
		byte status;
};

} // namespace openmsx

#endif
