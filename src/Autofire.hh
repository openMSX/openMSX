// $Id$

#ifndef __AUTOFIRE_HH__
#define __AUTOFIRE_HH__

#include "openmsx.hh"
#include "Settings.hh"

namespace openmsx {

class EmuTime;

/**
 * Autofire is a device that is between two other devices and outside
 * the bus. For example, between the keyboard and the ppi
 * or between a joyportconnecter and the psg
 *
 * There can be multiple autofire circuits. For example, one used
 * by the Ren-Sha Turbo and another one built into a joystick
 */
class Autofire : private SettingListener
{
public:
	Autofire(unsigned newMinInts, unsigned newMaxInts, const string& name);
	virtual ~Autofire();

	/**
	 * Give the output signal in negative logic
	 *
	 * When auto-fire is on, bit 0 will
	 * alternate between 0 and 1
	 *
	 * When auto-fire is off, bit 0 will be 0
	 */
	byte getSignal(const EmuTime &time);

private:
	virtual void update(const SettingLeafNode *setting);
	
	// Following two values specify the range of the autofire
	// as measured by the test program;
	//  the number of interrupts for 50 periods, measured
	//  in ntsc mode (which gives 60 interrupts per second)
	unsigned min_ints; // number of interrupts at fastest setting (>=1)
	unsigned max_ints; // number of interrupts at slowest setting (>=min_ints+1)

	IntegerSetting speedSetting; // the currently selected speed
	
	unsigned freq_divider; // derived from speed, min_ints and max_ints
};

} // namespace openmsx

#endif
