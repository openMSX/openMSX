// $Id$

#ifndef __MSXMEMDEVICE_HH__
#define __MSXMEMDEVICE_HH__

#include "MSXDevice.hh"

class MSXMemDevice : virtual public MSXDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXMemDevice();
		
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
		 * Register this device in all the slots that where specified
		 * in its config file
		 * Note: this is only a helper function, you do not have to use
		 *       this to register the device
		 */
		virtual void registerSlots();

		/**
		 * Test that the memory in the interval [start, start+length)
		 * is cacheable for reading. If it is, a pointer to a buffer
		 * containing this interval must be returned. If not, a null
		 * pointer must be returned.
		 * The default implementation always returns a null pointer.
		 * An interval will never cross a 16KB border.
		 * An interval will never contain the address 0xffff.
		 */
		virtual byte* getReadCacheLine(word start, word length);
		virtual byte* getWriteCacheLine(word start, word length);
};

#endif

