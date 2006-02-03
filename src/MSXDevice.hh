// $Id$

#ifndef MSXDEVICE_HH
#define MSXDEVICE_HH

#include "openmsx.hh"
#include "noncopyable.hh"
#include <string>
#include <vector>
#include <utility> // for pair

namespace openmsx {

class XMLElement;
class EmuTime;
class MSXMotherBoard;
class MSXDeviceCleanup;

/** An MSXDevice is an emulated hardware component connected to the bus
  * of the emulated MSX. There is no communication among devices, only
  * between devices and the CPU.
  */
class MSXDevice : private noncopyable
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
	 * Returns a human-readable name for this device.
	 * Default implementation is normally ok.
	 */
	virtual std::string getName() const;


	// IO

	/**
	 * Read a byte from an IO port at a certain time from this device.
	 * The default implementation returns 255.
	 */
	virtual byte readIO(word port, const EmuTime& time);

	/**
	 * Write a byte to a given IO port at a certain time to this
	 * device.
	 * The default implementation ignores the write (does nothing)
	 */
	virtual void writeIO(word port, byte value, const EmuTime& time);

	/**
	 * Read a byte from a given IO port. Reading via this method has no
	 * side effects (doesn't change the device status). If save reading
	 * is not possible this method returns 0xFF.
	 * This method is not used by the emulation. It can however be used
	 * by a debugger.
	 * The default implementation just returns 0xFF.
	 */
	virtual byte peekIO(word port, const EmuTime& time) const;


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
	 * Test that the memory in the interval [start, start+CACHE_LINE_SIZE)
	 * is cacheable for reading. If it is, a pointer to a buffer
	 * containing this interval must be returned. If not, a null
	 * pointer must be returned.
	 * Cacheable for reading means the data may be read directly
	 * from the buffer, thus bypassing the readMem() method, and
	 * thus also ignoring EmuTime.
	 * The default implementation always returns a null pointer.
	 * The start of the interval is CACHE_LINE_SIZE aligned.
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
	 * The start of the interval is CACHE_LINE_SIZE aligned.
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
	virtual byte peekMem(word address, const EmuTime& time) const;

	/** Get the mother board this device belongs to
	  */
	MSXMotherBoard& getMotherBoard() const { return motherBoard; }

	typedef std::vector<MSXDevice*> Devices;
	/** Get the device references that are specified for this device
	 */
	const Devices& getReferences() const;

protected:
	/** Every MSXDevice has a config entry; this constructor gets
	  * some device properties from that config entry.
	  * All subclasses must call this super-constructor.
	  * @param motherBoard the mother board this device belongs to
	  * @param config config entry for this device.
	  * @param time the moment in emulated time this MSXDevice is
	  *   created (typically at time zero: power-up).
	  * @param name The name for the MSXDevice (will be made unique)
	  */
	MSXDevice(MSXMotherBoard& motherBoard, const XMLElement& config,
	          const EmuTime& time, const std::string& name);
	MSXDevice(MSXMotherBoard& motherBoard, const XMLElement& config,
	          const EmuTime& time);

	const XMLElement& deviceConfig;
	friend class VDPIODelay;

	static byte unmappedRead[0x10000];	// Read only
	static byte unmappedWrite[0x10000];	// Write only

private:
	void staticInit();
	void init(const std::string& name);
	void deinit();

	void lockDevices();
	void unlockDevices();

	void registerSlots(const XMLElement& config);
	void unregisterSlots();

	void registerPorts(const XMLElement& config);
	void unregisterPorts();

	typedef std::vector<std::pair<unsigned, unsigned> > MemRegions;
	MemRegions memRegions;
	int ps;
	int ss;
	int externalSlotID;
	std::vector<byte> inPorts;
	std::vector<byte> outPorts;

	MSXMotherBoard& motherBoard;
	std::string deviceName;

	Devices references;
	Devices referencedBy;

	friend class MSXDeviceCleanup;
	std::auto_ptr<MSXDeviceCleanup> cleanup; // must be last
};

} // namespace openmsx

#endif
