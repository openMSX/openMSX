// $Id$

#ifndef __MSXDEVICE_HH__
#define __MSXDEVICE_HH__

#include <string>
#include "openmsx.hh"

namespace openmsx {

class XMLElement;
class EmuTime;

/** An MSXDevice is an emulated hardware component connected to the bus
  * of the emulated MSX. There is no communication among devices, only
  * between devices and the CPU.
  */
class MSXDevice
{
public:
	virtual ~MSXDevice() = 0;

	/**
	 * This method is called on reset.
	 * Default implementation does nothing.
	 */
	virtual void reset(const EmuTime& time);

	/**
	 * This method is called when MSX is powered down. The default
	 * implementation does nothing, this is usually ok. Typically devices
	 * that need to turn off LEDs need to reimplement this method.
	 * @param time The moment in time the power down occurs.
	 */
	virtual void powerDown(const EmuTime& time);

	/**
	 * This method is called when MSX is powered up. The default
	 * implementation calls reset(), this is usually ok. 
	 * @param time The moment in time the power up occurs.
	 */
	virtual void powerUp(const EmuTime& time);

	/**
	 * Returns a human-readable name for this device. The name is set
	 * in the setConfigDevice() method. This method is mostly used to
	 * print debug info.
	 * Default implementation is normally ok.
	 */
	virtual const std::string& getName() const;

	
	// IO
	
	/**
	 * Read a byte from an IO port at a certain time from this device.
	 * The default implementation returns 255.
	 */
	virtual byte readIO(byte port, const EmuTime& time);

	/**
	 * Write a byte to a given IO port at a certain time to this
	 * device.
	 * The default implementation ignores the write (does nothing)
	 */
	virtual void writeIO(byte port, byte value, const EmuTime& time);


	// Memory 
	
	/**
	 * Read a byte from a location at a certain time from this
	 * device.
	 * The default implementation returns 255.
	 */
	virtual byte readMem(word address, const EmuTime& time);

	/**
	 * Write a given byte to a given location at a certain time 
	 * to this device.
	 * The default implementation ignores the write (does nothing).
	 */
	virtual void writeMem(word address, byte value, const EmuTime& time);
	
	/**
	 * Read a byte from a given IO port. Reading via this method has no
	 * side effects (doesn't change the device status). If save reading
	 * is not possible this method returns 0xFF.
	 * This method is not used by the emulation. It can however be used
	 * by a debugger.
	 * The default implementation just returns 0xFF.
	 */
	virtual byte peekIO(byte port, const EmuTime& time) const;
	
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
	/** Every MSXDevice has a config entry; this constructor gets
	  * some device properties from that config entry.
	  * All subclasses must call this super-constructor.
	  * @param config config entry for this device.
	  * @param time the moment in emulated time this MSXDevice is
	  *   created (typically at time zero: power-up).
	  */
	MSXDevice(const XMLElement& config, const EmuTime& time);

	const XMLElement& deviceConfig;
	friend class VDPIODelay;
	
	static byte unmappedRead[0x10000];	// Read only
	static byte unmappedWrite[0x10000];	// Write only

private:
	void initMem();
	void registerSlots(const XMLElement& config);
	void unregisterSlots();

	void registerPorts(const XMLElement& config);
	void unregisterPorts(const XMLElement& config);

	int ps;
	int ss;
	int pages;
};

} // namespace openmsx

#endif //__MSXDEVICE_HH__
