// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "Scheduler.hh"
#include "Z80.hh"
//#include "MSXR800.hh"

class MSXCPU : public MSXDevice, public Schedulable
{
	friend MSXRomPatchInterface;
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
		
		CPU* getActiveCPU();
		
		const EmuTime &getTargetTime();

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
