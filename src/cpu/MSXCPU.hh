// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "MSXDevice.hh"

// forward declaration
class CPU;
class Z80;
class R800;


class MSXCPU : public MSXDevice
{
	public:
		enum CPUType { CPU_Z80, CPU_R800 };
	
		/**
		 * Destructor
		 */
		virtual ~MSXCPU();

		/**
		 * This is a singleton class. This method returns a reference
		 * to the single instance of this class.
		 */
		static MSXCPU *instance();
	
		virtual void reset(const EmuTime &time);
		
		const EmuTime &getCurrentTime() const;
		void setActiveCPU(CPUType cpu);
		
		/**
		 * Invalidate the CPU its cache for the interval 
		 * [start, start+num*CACHE_LINE_SIZE)
		 * For example MSXMemoryMapper and MSXGameCartrigde need to call this
		 * method when a 'memory switch' occurs.
		 */
		void invalidateCache(word start, int num);

		/**
		 * This method raises an interrupt. A device may call this
		 * method more than once. If the device wants to lower the
		 * interrupt again it must call the lowerIRQ() method exactly as
		 * many times.
		 * Before using this method take a look at IRQHelper
		 */
		void raiseIRQ();

		/**
		 * This methods lowers the interrupt again. A device may never
		 * call this method more often than it called the method
		 * raiseIRQ().
		 * Before using this method take a look at IRQHelper
		 */
		void lowerIRQ();

	private:
		/**
		 * Constructor.
		 */
		MSXCPU(Device *config, const EmuTime &time);
		
		// only for Scheduler
		void executeUntilTarget(const EmuTime &time);
		void setTargetTime(const EmuTime &time);
		const EmuTime &getTargetTime() const;
		friend class Scheduler;

		static MSXCPU *oneInstance;

		Z80 *z80;
		R800 *r800;
	
		CPU *activeCPU;
};
#endif //__MSXCPU_HH__
