// $Id: CPU.hh,v

#ifndef __CPU_HH__
#define __CPU_HH__

#include "emutime.hh"
#include "CPUInterface.hh"

class CPU 
{
	public:
		/**
		 * Destructor
		 */
		virtual ~CPU();

		/**
		 * Reset the CPU
		 */
		virtual void reset() = 0;

		/**
		 * Emulated CPU till a given target-time. This implicitly calls
		 * the method setTargetTime()
		 */
		void executeUntilTarget(const Emutime &time);
		
		/**
		 * Sets the CPU CPU its current time. This is used to 'warp' a CPU
		 * when you switch between Z80/R800
		 */
		void setCurrentTime(const Emutime &time);
		
		/**
		 * Returns the CPU its current time
		 */
		const Emutime &getCurrentTime();
		
		/**
		 * Alter the target time. The Scheduler uses this to announce
		 * changes in the scheduling
		 */
		void setTargetTime(const Emutime &time);

		/**
		 * Returns the previously set target time
		 */
		const Emutime &getTargetTime();
	
	protected:
		/*
		 * Constructor
		 */
		CPU(CPUInterface *interf, int clockFreq);
		
		/*
		 * Emulate CPU till a previously set target time.
		 */
		virtual void execute() = 0;
		
		/*
		 * Instance variables
		 */
		CPUInterface *interface;
		Emutime targetTime;
		Emutime currentTime;
		bool targetChanged;	// optimization
};
#endif //__CPU_HH__
