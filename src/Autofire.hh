// $Id$

#ifndef __AUTOFIRE_HH__
#define __AUTOFIRE_HH__

#include "openmsx.hh"
#include "EmuTime.hh"

namespace openmsx {

/*
 * Autofire is a device that is between two other devices and outside
 * the bus. For example, between the keyboard and the ppi
 * or between a joyportconnecter and the psg
 *
 * There can be multiple autofire circuits. For example, one used
 * by the Ren-Sha Turbo and another one built into a joystick
 *
 */
class Autofire
{
	public:
                /**
                 * Constructor
                 */
                Autofire(int newMinInts, int newMaxInts);

                /**
                 * Destructor
                 */
                ~Autofire();

                /**
                  * Give the output signal in negative logic
                  *
                  * When auto-fire is on, bit 0 will
                  * alternate between 0 and 1
                  *
                  * When auto-fire is off, bit 0 will be 0
                  */
                byte getSignal(const EmuTime &time);

		/**
		  * Increase the speed with one value
                  *
		  */
            	void increaseSpeed();

		/**
		  * Decrease the speed with one value
		  */
		void decreaseSpeed();
 
		/**
		  * Get the actual speed value
		  */
		int getSpeed();
 
		/**
		  * Set the actual speed value
		  */
		void setSpeed(const int newSpeed);

	private:
                // State
		int speed; /* 0: off, 1-100: speed indicator */
		int old_speed; /* old value of speed */
		int freq_divider; /* derived from speed, min_ints and max_ints */

                // Following two values specify the range of the autofire
                // as measured by the test program;
                //  the number of interrupts for 50 periods, measured
                //  in ntsc mode (which gives 60 interrupts per second)
		int min_ints; /* number of interrupts at fastest setting (>=1) */
		int max_ints; /* number of interrupts at slowest setting (>=min_ints+1) */
};

} // namespace openmsx

#endif
