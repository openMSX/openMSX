// $Id: CPU.hh,v

#ifndef __CPU_HH__
#define __CPU_HH__

#include "EmuTime.hh"
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
		void executeUntilTarget(const EmuTime &time);
		
		/**
		 * Sets the CPU CPU its current time. This is used to 'warp' a CPU
		 * when you switch between Z80/R800
		 */
		void setCurrentTime(const EmuTime &time);
		
		/**
		 * Returns the CPU its current time
		 */
		const EmuTime &getCurrentTime();
		
		/**
		 * Alter the target time. The Scheduler uses this to announce
		 * changes in the scheduling
		 */
		void setTargetTime(const EmuTime &time);

		/**
		 * Returns the previously set target time
		 */
		const EmuTime &getTargetTime();
	
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
		EmuTime targetTime;
		EmuTime currentTime;
		bool targetChanged;	// optimization
};
#endif //__CPU_HH__
