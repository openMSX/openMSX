// $Id$

#ifndef __MSXCPU_HH__
#define __MSXCPU_HH__

#include "MSXDevice.hh"
#include "CPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "CPU.hh"
#include "Z80.hh"
//#include "MSXR800.hh"

class MSXCPU : public MSXDevice
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
	
		// MSXDevice
		void reset(const EmuTime &time);
		
		// MSXCPU
		void executeUntilTarget(const EmuTime &time);
		void setTargetTime(const EmuTime &time);
		const EmuTime &getCurrentTime();
		
		void setActiveCPU(CPUType cpu);
		const EmuTime &getTargetTime();

	private:
		/**
		 * Constructor.
		 */
		MSXCPU(MSXConfig::Device *config);

		static MSXCPU *oneInstance;
		void executeUntilEmuTime(const EmuTime &time); // prevent use

		Z80 *z80;
		//MSXR800 *r800;
		CPU *activeCPU;
};
#endif //__MSXCPU_HH__
