// $Id$

#ifndef __CPU_HH__
#define __CPU_HH__

#include "EmuTime.hh"
#include "CPUInterface.hh"

class CPU 
{
	typedef union {
	#ifdef LSB_FIRST
	   struct { byte l,h; } B;
	#else
	   struct { byte h,l; } B;
	#endif
	   word w;
	} z80regpair;

	struct CPURegs {
		z80regpair AF,  BC,  DE,  HL, IX, IY, PC, SP;
		z80regpair AF2, BC2, DE2, HL2;
		bool nextIFF1, IFF1, IFF2, HALT;
		byte IM, I;
		byte R, R2;	// refresh = R&127 | R2&128
	};

	typedef signed char offset;

	public:
		/**
		 * Destructor
		 */
		virtual ~CPU();

		/**
		 * Reset the CPU
		 */
		virtual void reset(const EmuTime &time) = 0;

		/**
		 * Emulated CPU till a given target-time. This implicitly calls
		 * the method setTargetTime()
		 */
		void executeUntilTarget(const EmuTime &time);
		
		/**
		 * Sets the CPU its current time. This is used to 'warp' a CPU
		 * when you switch between Z80/R800
		 */
		virtual void setCurrentTime(const EmuTime &time) = 0;
		
		/**
		 * Returns the CPU its current time
		 */
		virtual const EmuTime &getCurrentTime() = 0;
		
		/**
		 * Alter the target time. The Scheduler uses this to announce
		 * changes in the scheduling
		 */
		void setTargetTime(const EmuTime &time);

		/**
		 * Returns the previously set target time
		 */
		const EmuTime &getTargetTime();

		/**
		 * Get the current CPU registers
		 */
		const CPURegs& getCPURegs();

		/**
		 * Set the current CPU registers
		 */
		void setCPURegs(const CPURegs &regs);

	protected:
		/*
		 * Constructor
		 */
		CPU(CPUInterface *interf);
		
		/*
		 * Emulate CPU till a previously set target time.
		 */
		virtual void execute() = 0;
		
		/*
		 * Instance variables
		 */
		CPUInterface *interface;
		EmuTime targetTime;
		bool targetChanged;	// optimization

		CPURegs R;
};
#endif //__CPU_HH__

