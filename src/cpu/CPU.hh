// $Id$

#ifndef __CPU_HH__
#define __CPU_HH__

#include "openmsx.hh"
#include "EmuTime.hh"

#ifdef DEBUG
#define CPU_DEBUG
#endif


// forward declarations
class BooleanSetting;
class CPUInterface;

typedef signed char offset;


class CPU
{
	friend class MSXCPU;
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

		virtual ~CPU();
		/**
		 * Reset the CPU.
		 */
		void reset(const EmuTime &time);

		/**
		 * Emulated CPU till a given target-time.
		 * This implicitly calls the method setTargetTime().
		 */
		void executeUntilTarget(const EmuTime &time);

		/**
		 * Sets the CPU its current time.
		 * This is used to 'warp' a CPU when you switch between Z80/R800.
		 */
		virtual void setCurrentTime(const EmuTime &time) = 0;

		/**
		 * Returns the CPU its current time.
		 */
		virtual const EmuTime &getCurrentTime() const = 0;

		/**
		 * Alter the target time.
		 * The Scheduler uses this to announce changes in the scheduling.
		 */
		void setTargetTime(const EmuTime &time);

		/**
		 * Get the target time. Only used to switch active CPU.
		 */
		const EmuTime &getTargetTime() const;

		/**
		 * Wait
		 */
		void wait(const EmuTime &time);

		/**
		 * Read a byte from memory. If possible the byte is read from
		 * cache, otherwise the readMem() method of CPUInterface is used.
		 */
		byte readMem(word address);

		/**
		 * Write a byte from memory. If possible the byte is written to
		 * cache, otherwise the writeMem() method of CPUInterface is used.
		 */
		void writeMem(word address, byte value);

		/**
		 * Invalidate the CPU its cache for the interval
		 * [start, start+num*CACHE_LINE_SIZE).
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
		static const int CACHE_LINE_BITS = 8;	// 256 bytes
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
		/** Create a new CPU.
		  * @param interf Interface between this CPU and the bus.
		  */
		CPU(CPUInterface *interf);

		/** Emulate CPU till a previously set target time,
		  * the target may change (become smaller) during emulation.
		  */
		virtual void executeCore() = 0;

		/** Reset CPU core.
		  */
		virtual void resetCore() = 0;

		void makeTables();

		// state machine variables
		CPURegs R;
		int slowInstructions;
		int IRQStatus;

		z80regpair x,y;
		z80regpair memptr;
		byte bit;
		byte dummy;
		offset ofst;
		byte* p;
		bool repeat, increase;
		bool stop;

		CPUInterface *interface;
		EmuTime targetTime;

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

		// memory cache
		const byte* readCacheLine[CACHE_LINE_NUM];
		byte* writeCacheLine[CACHE_LINE_NUM];
		bool readCacheTried [CACHE_LINE_NUM];
		bool writeCacheTried[CACHE_LINE_NUM];
	
	#ifdef CPU_DEBUG
		static BooleanSetting *traceSetting;

	public:
		byte debugmemory[65536];
		char to_print_string[300];
	#endif
};
#endif //__CPU_HH__
