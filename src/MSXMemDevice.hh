// $Id$

#ifndef __MSXMEMDEVICE_HH__
#define __MSXMEMDEVICE_HH__

#include "MSXDevice.hh"
#include "openmsx.hh"

class MSXMemDevice : virtual public MSXDevice
{
	public:
		MSXMemDevice(Device *config, const EmuTime &time);
		virtual ~MSXMemDevice() = 0;
		
		/**
		 * Read a byte from a location at a certain time from this
		 * device.
		 * The default implementation returns 255.
		 */
		virtual byte readMem(word address, const EmuTime &time);

		/**
		 * Write a given byte to a given location at a certain time 
		 * to this device.
		 * The default implementation ignores the write (does nothing).
		 */
		virtual void writeMem(word address, byte value, const EmuTime &time);
		
		/**
		 * Test that the memory in the interval [start, start+CACHE_LINE_SIZE)
		 * is cacheable for reading. If it is, a pointer to a buffer
		 * containing this interval must be returned. If not, a null
		 * pointer must be returned.
		 * Cacheable for reading means the data may be read directly 
		 * from the buffer, thus bypassing the readMem() method, and
		 * thus also ignoring EmuTime.
		 * The default implementation always returns a null pointer.
		 * The start of the interval is CACHE_LINE_SIZE alligned.
		 */
		virtual const byte* getReadCacheLine(word start) const;
		
		/**
		 * Test that the memory in the interval [start, start+CACHE_LINE_SIZE)
		 * is cacheable for writing. If it is, a pointer to a buffer
		 * containing this interval must be returned. If not, a null
		 * pointer must be returned.
		 * Cacheable for writing means the data may be written directly 
		 * to the buffer, thus bypassing the writeMem() method, and
		 * thus also ignoring EmuTime.
		 * The default implementation always returns a null pointer.
		 * The start of the interval is CACHE_LINE_SIZE alligned.
		 */
		virtual byte* getWriteCacheLine(word start) const;

		/**
		 * Read a byte from a given memory location. Reading memory
		 * via this method has no side effects (doesn't change the
		 * device status). If save reading is not possible this
		 * method returns 0xFF.
		 * This method is not used by the emulation. It can however
		 * be used by a debugger.
		 * The default implementation uses the cache mechanism
		 * (getReadCacheLine() method). If a certain region is not
		 * cacheable you cannot read it by default, Override this
		 * method if you want to improve this behaviour.
		 */
		virtual byte peekMem(word address) const;
	
	protected:
		static byte unmappedRead[0x10000];	// Read only
		static byte unmappedWrite[0x10000];	// Write only
	
	private:
		void init();
		
		/**
		 * Register this device in all the slots that where specified
		 * in its config file. This method is called by the constructor,
		 */
		virtual void registerSlots();

};

#endif

