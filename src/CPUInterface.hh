// $Id: CPUInterface.hh,v

#ifndef __CPUINTERFACE_HH__
#define __CPUINTERFACE_HH__

#include "openmsx.hh"
#include "emutime.hh"

class CPUInterface {
	public:
		/**
		 * Memory and IO read/write operations
		 */
		virtual byte readIO   (word port, Emutime &time) = 0;
		virtual void writeIO  (word port,byte value, Emutime &time) = 0;
		virtual byte readMem  (word address, Emutime &time) = 0;
		virtual void writeMem (word address, byte value, Emutime &time) = 0;

		/**
		 * Returns true when INT line is active
		 */
		virtual bool IRQStatus() = 0;

		/**
		 * CPU uses this method to read 'extra' data from the databus
		 * used in interrupt routines. In MSX this returns always 255.
		 */
		virtual byte dataBus();
		
		/**
		 * Returns true when there was a rising edge on the NMI line
		 * (rising = non-active -> active)
		 */
		bool NMIEdge();

		/**
		 * Called when ED FE occurs. Can be used
		 * to emulated disk access etc.
		 */
		virtual void patch();
		
		/**
		 * Called when RETI accurs
		 */
		virtual void reti();
		
		/**
		 * Called when RETN occurs
		 */
		virtual void retn();

	protected:
		/*
		 * Constructor
		 */
		CPUInterface();
	
		/*
		 * Returns true when NMI line is active.
		 * On MSX this line is never active.
		 */
		virtual bool NMIStatus();

	private:
		bool prevNMIStat;
};

#endif
