// $Id$

#ifndef AUTOFIRE_HH
#define AUTOFIRE_HH

#include "SettingListener.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class IntegerSetting;

/**
 * Autofire is a device that is between two other devices and outside
 * the bus. For example, between the keyboard and the PPI
 * or between a joyport connecter and the PSG.
 *
 * There can be multiple autofire circuits. For example, one used
 * by the Ren-Sha Turbo and another one built into a joystick.
 */
class Autofire : private SettingListener
{
public:
	Autofire(unsigned newMinInts, unsigned newMaxInts,
	         const std::string& name);
	virtual ~Autofire();

	/**
	 * Give the output signal in negative logic.
	 *
	 * When auto-fire is on, bit 0 will
	 * alternate between 0 and 1.
	 *
	 * When auto-fire is off, bit 0 will be 0.
	 */
	byte getSignal(const EmuTime& time);

private:
	/** Sets the clock frequency according to the current value of the speed
	  * settings.
	  */
	void setClock();

	virtual void update(const Setting* setting);

	// Following two values specify the range of the autofire
	// as measured by the test program:
	/** Number of interrupts at fastest setting (>=1).
	  * The number of interrupts for 50 periods, measured
	  * in ntsc mode (which gives 60 interrupts per second).
	  */
	unsigned min_ints;
	/** Number of interrupts at slowest setting (>=min_ints+1).
	  * The number of interrupts for 50 periods, measured
	  * in ntsc mode (which gives 60 interrupts per second).
	  */
	unsigned max_ints;

	/** The currently selected speed. */
	std::auto_ptr<IntegerSetting> speedSetting;

	/** Each tick of this clock, the signal changes.
	  * Frequency is derived from speed, min_ints and max_ints.
	  */
	DynamicClock clock;
};

} // namespace openmsx

#endif
