// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "CPU.hh"

// forward declaration
class CPU;
class Z80;
//class R800;


class MSXCPU : public MSXDevice, public Schedulable
{
	public:
		enum CPUType { CPU_Z80 };	//, CPU_R800 };
	
		/**
		 * Destructor
		 */
		virtual ~MSXCPU();

		/**
		 * This is a singleton class. This method returns a reference
		 * to the single instance of this class.
		 */
		static MSXCPU *instance();
	
		void reset(const EmuTime &time);
		
		// MSXCPU
		void executeUntilTarget(const EmuTime &time);
		void setTargetTime(const EmuTime &time);
		const EmuTime &getCurrentTime();
		
		void setActiveCPU(CPUType cpu);
		
		const EmuTime &getTargetTime();

		/**
		 * Get the current CPU registers.
		 * This method return a non-const alias, this means it can
		 * also be used to change the CPU registers.
		 */
		CPU::CPURegs& getCPURegs();

		/**
		 * Invalidate the CPU its cache for the interval 
		 * [start, start+num*CACHE_LINE_SIZE)
		 * For example MSXMemoryMapper and MSXGameCartrigde need to call this
		 * method when a 'memory switch' occurs.
		 */
		void invalidateCache(word start, int num);

	private:
		/**
		 * Constructor.
		 */
		MSXCPU(MSXConfig::Device *config, const EmuTime &time);

		static MSXCPU *oneInstance;
		void executeUntilEmuTime(const EmuTime &time, int userData); // prevent use

		Z80 *z80;
		//MSXR800 *r800;
	
		CPU *activeCPU;
};
#endif //__MSXCPU_HH__
