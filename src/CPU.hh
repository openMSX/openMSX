// $Id$

#ifndef __CPU_HH__
#define __CPU_HH__

#include "openmsx.hh"
#include "EmuTime.hh"

// forward declaration
class CPUInterface;

typedef signed char offset;


class CPU
{
	#ifndef WORDS_BIGENDIAN
		#define LSB_FIRST
	#endif

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


		/**
		 * Read a byte from memory. If possible the byte is read from 
		 * cache, otherwise the readMem() method of CPUInterface is used.
		 */
		byte CPU::readMem(word address);
		
		/**
		 * Write a byte from memory. If possible the byte is written to 
		 * cache, otherwise the writeMem() method of CPUInterface is used.
		 */
		void CPU::writeMem(word address, byte value);

		/**
		 * Invalidate the CPU its cache for the interval 
		 * [start, start+num*CACHE_LINE_SIZE)
		 */
		void invalidateCache(word start, int num);


		static const int CACHE_LINE_BITS = 8;	// 256bytes
		static const int CACHE_LINE_SIZE = 1 << CACHE_LINE_BITS;
		static const int CACHE_LINE_NUM  = 0x10000 / CACHE_LINE_SIZE;
		static const int CACHE_LINE_LOW  = CACHE_LINE_SIZE - 1;
		static const int CACHE_LINE_HIGH = 0xffff - CACHE_LINE_LOW;

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

	private:
		byte* readCacheLine [CACHE_LINE_NUM];
		byte* writeCacheLine[CACHE_LINE_NUM];
		bool readCacheTried [CACHE_LINE_NUM];
		bool writeCacheTried[CACHE_LINE_NUM];
};
#endif //__CPU_HH__

