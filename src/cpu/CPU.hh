// $Id$

#ifndef __CPU_HH__
#define __CPU_HH__

#include "openmsx.hh"
#include "EmuTime.hh"

#ifdef DEBUG
#define CPU_DEBUG
#endif


// forward declaration
class CPUInterface;

typedef signed char offset;

class CPU
{
	public:
		typedef union {
		#ifndef WORDS_BIGENDIAN
		   struct { byte l,h; } B;
		#else
		   struct { byte h,l; } B;
		#endif
		   word w;
		} z80regpair;

		class CPURegs {
		public:
			z80regpair AF,  BC,  DE,  HL, IX, IY, PC, SP;
			z80regpair AF2, BC2, DE2, HL2;
			bool nextIFF1, IFF1, IFF2, HALT;
			byte IM, I;
			byte R, R2;	// refresh = R&127 | R2&128

			void ei() { IFF1 = nextIFF1 = IFF2 = true; }
			void di() { IFF1 = nextIFF1 = IFF2 = false; }
		};

		/**
		 * Destructor
		 */
		virtual ~CPU();

		/**
		 * Reset the CPU
		 */
		void reset(const EmuTime &time);

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


		/**
		 * This method raises an interrupt. A device may call this
		 * method more than once. If the device wants to lower the
		 * interrupt again it must call the lowerIRQ() method exactly as
		 * many times.
		 */
		void raiseIRQ();

		/**
		 * This methods lowers the interrupt again. A device may never
		 * call this method more often than it called the method
		 * raiseIRQ().
		 */
		void lowerIRQ();


		// cache constants
		static const int CACHE_LINE_BITS = 8;	// 256bytes
		static const int CACHE_LINE_SIZE = 1 << CACHE_LINE_BITS;
		static const int CACHE_LINE_NUM  = 0x10000 / CACHE_LINE_SIZE;
		static const int CACHE_LINE_LOW  = CACHE_LINE_SIZE - 1;
		static const int CACHE_LINE_HIGH = 0xffff - CACHE_LINE_LOW;

		// flag positions
		static const byte S_FLAG = 0x80;
		static const byte Z_FLAG = 0x40;
		static const byte Y_FLAG = 0x20;
		static const byte H_FLAG = 0x10;
		static const byte X_FLAG = 0x08;
		static const byte V_FLAG = 0x04;
		static const byte P_FLAG = V_FLAG;
		static const byte N_FLAG = 0x02;
		static const byte C_FLAG = 0x01;

	protected:
		/*
		 * Constructor
		 */
		CPU(CPUInterface *interf);
		
		/*
		 * Emulate CPU till a previously set target time, the target
		 * may change (become smaller) during emulation
		 */
		virtual void execute() = 0;
		
		void makeTables();
		
		/*
		 * Instance variables
		 */
		CPUInterface *interface;
		EmuTime targetTime;
		CPURegs R;
		int slowInstructions;
		int IRQStatus;

		// flag-register tables
		static byte ZSTable[256];
		static byte ZSXYTable[256];
		static byte ZSPXYTable[256];
		static byte ZSPTable[256];
		
		// instruction lookup tables
		static const word DAATable[2048];
		static const byte irep_tmp1[4][4];
		static const byte drep_tmp1[4][4];
		static const byte breg_tmp2[256];
	
	private:
		byte* readCacheLine [CACHE_LINE_NUM];
		byte* writeCacheLine[CACHE_LINE_NUM];
		bool readCacheTried [CACHE_LINE_NUM];
		bool writeCacheTried[CACHE_LINE_NUM];
};
#endif //__CPU_HH__

