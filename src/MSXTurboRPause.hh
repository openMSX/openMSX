// $Id$

#ifndef __TURBORPAUSE_HH__
#define __TURBORPAUSE_HH__

#include "MSXDevice.hh"
#include "BooleanSetting.hh"

namespace openmsx {

/**
 * This class implements the MSX Turbo-R pause key
 *
 *  Whenever the pause key is pressed a flip-flop is toggled.
 *  The status of this flip-flop can be read from io-port 0xA7.
 *   bit 0 indicates the status (1 = pause active)
 *   all other bits read 0
 */
class MSXTurboRPause : public MSXDevice
{
public:
	MSXTurboRPause(const XMLElement& config, const EmuTime& time);
	virtual ~MSXTurboRPause();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);

private:
	BooleanSetting turboRPauseSetting;
};

} // namespace openmsx

#endif // __TURBORPAUSE_HH__
