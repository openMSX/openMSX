// $Id$

#ifndef __RENSHATURBO_HH__
#define __RENSHATURBO_HH__

#include "openmsx.hh"
#include "EmuTime.hh"
#include "Command.hh"
#include "Autofire.hh"

namespace openmsx {

/*
 * Ren-Sha Turbo is the autofire in several MSX 2+ models and in
 * the MSX turbo R. It works on space bar and on fire-button 1 of
 * both joystick ports
 *
 * It uses one autofire circuit.
 *
 */
class RenShaTurbo
{
	public:
                /**
                 * Destructor
                 */
                ~RenShaTurbo();

                /**
                  * This is a singleton class
                  */
                static RenShaTurbo* instance();

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
                RenShaTurbo();

		class SpeedCmd : public Command {
			virtual void execute(const std::vector<std::string> &tokens);
			virtual void help(const std::vector<std::string> &tokens) const;
		};

		// Commands
                SpeedCmd speedCmd;

                // State
		bool is_present; /* true when autofire present */

		// The Autofire circuit
		Autofire* autofire;
};

} // namespace openmsx

#endif
