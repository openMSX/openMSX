// $Id$

#ifndef __CPUINTERFACE_HH__
#define __CPUINTERFACE_HH__

#include <list>
#include "openmsx.hh"
#include "CPU.hh"

// forward declaration
class MSXRomPatchInterface;
class EmuTime;


class CPUInterface {
	public:
		/**
		 * Memory and IO read/write operations
		 */
		virtual byte readIO   (word port, const EmuTime &time) = 0;
		virtual void writeIO  (word port,byte value, const EmuTime &time) = 0;
		virtual byte readMem  (word address, const EmuTime &time) = 0;
		virtual void writeMem (word address, byte value, const EmuTime &time) = 0;

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
		void patch(CPU::CPURegs& regs);

		/**
		 * (Un)register a MSXRomPatchInterface
		 */
		void   registerInterface(MSXRomPatchInterface *i);
		void unregisterInterface(MSXRomPatchInterface *i);

		/**
		 * Called when RETI accurs
		 */
		virtual void reti(CPU::CPURegs& regs);
		
		/**
		 * Called when RETN occurs
		 */
		virtual void retn(CPU::CPURegs& regs);

		/*
		 * Destructor
		 */
		 virtual ~CPUInterface();

		/**
		 * Test that the memory in the interval [start, start+CACHE_LINE_SIZE)
		 * is cacheable for reading. If it is, a pointer to a buffer
		 * containing this interval must be returned. If not, a null
		 * pointer must be returned.
		 * Cacheable for reading means the data may be read directly
		 * from the buffer, thus bypassing the readMem() method, and
		 * thus also ignoring EmuTime.
		 * The default implementation always returns a null pointer.
		 * An interval will never cross a 16KB border.
		 * An interval will never contain the address 0xffff.
		 */
		virtual byte* getReadCacheLine(word start) = 0;

		/**
		 * Test that the memory in the interval [start, start+CACHE_LINE_SIZE)
		 * is cacheable for writing. If it is, a pointer to a buffer
		 * containing this interval must be returned. If not, a null
		 * pointer must be returned.
		 * Cacheable for writing means the data may be written directly
		 * to the buffer, thus bypassing the writeMem() method, and
		 * thus also ignoring EmuTime.
		 * The default implementation always returns a null pointer.
		 * An interval will never cross a 16KB border.
		 * An interval will never contain the address 0xffff.
		 */
		virtual byte* getWriteCacheLine(word start) = 0;
	
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
		std::list<MSXRomPatchInterface*> romPatchInterfaceList;
};

#endif
