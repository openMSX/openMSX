#ifndef MSXDEVICE_HH
#define MSXDEVICE_HH

#include "DeviceConfig.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <string>
#include <vector>
#include <utility> // for pair

namespace openmsx {

class XMLElement;
class MSXMotherBoard;
class MSXCPU;
class MSXCPUInterface;
class Scheduler;
class CliComm;
class Reactor;
class CommandController;
class LedStatus;
class PluggingController;
class HardwareConfig;
class TclObject;

/** An MSXDevice is an emulated hardware component connected to the bus
  * of the emulated MSX. There is no communication among devices, only
  * between devices and the CPU.
  */
class MSXDevice
{
public:
	MSXDevice(const MSXDevice&) = delete;
	MSXDevice& operator=(const MSXDevice&) = delete;

	using Devices = std::vector<MSXDevice*>;

	virtual ~MSXDevice() = 0;

	/** Returns the hardwareconfig this device belongs to.
	  */
	const HardwareConfig& getHardwareConfig() const {
		return deviceConfig.getHardwareConfig();
	}

	/** Checks whether this device can be removed (no other device has a
	  * reference to it). Throws an exception if it can't be removed.
	  */
	void testRemove(Devices alreadyRemoved) const;

	/**
	 * This method is called on reset.
	 * Default implementation does nothing.
	 */
	virtual void reset(EmuTime::param time);

	/**
	 * Gets IRQ vector used in IM2. This method only exists to support
	 * YamahaSfg05. There is no way for several devices to coordinate which
	 * vector is actually send to the CPU. But this IM is anyway not really
	 * supported in the MSX standard.
	 * Default implementation returns 0xFF.
	 */
	virtual byte readIRQVector();

	/**
	 * This method is called when MSX is powered down. The default
	 * implementation does nothing, this is usually ok. Typically devices
	 * that need to turn off LEDs need to reimplement this method.
	 * @param time The moment in time the power down occurs.
	 */
	virtual void powerDown(EmuTime::param time);

	/**
	 * This method is called when MSX is powered up. The default
	 * implementation calls reset(), this is usually ok.
	 * @param time The moment in time the power up occurs.
	 */
	virtual void powerUp(EmuTime::param time);

	/**
	 * Returns a human-readable name for this device.
	 * Default implementation is normally ok.
	 */
	virtual std::string getName() const;

	/** Returns list of name(s) of this device.
	 * This is normally the same as getName() (but formatted as a Tcl list)
	 * except for multi-{mem,io}-devices.
	 */
	virtual void getNameList(TclObject& result) const;

	/** Get device info.
	  * Used by the 'machine_info device' command.
	  */
	void getDeviceInfo(TclObject& result) const;

	/** Returns the range where this device is visible in memory.
	  * This is the union of the "mem" tags inside the device tag in
	  * hardwareconfig.xml (though practically always there is only one
	  * "mem" tag). Information on possible holes in this range (when there
	  * are multiple "mem" tags) is not returned by this method.
	  */
	void getVisibleMemRegion(unsigned& base, unsigned& size) const;

	// IO

	/**
	 * Read a byte from an IO port at a certain time from this device.
	 * The default implementation returns 255.
	 */
	virtual byte readIO(word port, EmuTime::param time);

	/**
	 * Write a byte to a given IO port at a certain time to this
	 * device.
	 * The default implementation ignores the write (does nothing)
	 */
	virtual void writeIO(word port, byte value, EmuTime::param time);

	/**
	 * Read a byte from a given IO port. Reading via this method has no
	 * side effects (doesn't change the device status). If save reading
	 * is not possible this method returns 0xFF.
	 * This method is not used by the emulation. It can however be used
	 * by a debugger.
	 * The default implementation just returns 0xFF.
	 */
	virtual byte peekIO(word port, EmuTime::param time) const;


	// Memory

	/**
	 * Read a byte from a location at a certain time from this
	 * device.
	 * The default implementation returns 255.
	 */
	virtual byte readMem(word address, EmuTime::param time);

	/**
	 * Write a given byte to a given location at a certain time
	 * to this device.
	 * The default implementation ignores the write (does nothing).
	 */
	virtual void writeMem(word address, byte value, EmuTime::param time);

	/**
	 * Test that the memory in the interval [start, start +
	 * CacheLine::SIZE) is cacheable for reading. If it is, a pointer to a
	 * buffer containing this interval must be returned. If not, a null
	 * pointer must be returned.
	 * Cacheable for reading means the data may be read directly
	 * from the buffer, thus bypassing the readMem() method, and
	 * thus also ignoring EmuTime.
	 * The default implementation always returns a null pointer.
	 * The start of the interval is CacheLine::SIZE aligned.
	 */
	virtual const byte* getReadCacheLine(word start) const;

	/**
	 * Test that the memory in the interval [start, start +
	 * CacheLine::SIZE) is cacheable for writing. If it is, a pointer to a
	 * buffer containing this interval must be returned. If not, a null
	 * pointer must be returned.
	 * Cacheable for writing means the data may be written directly
	 * to the buffer, thus bypassing the writeMem() method, and
	 * thus also ignoring EmuTime.
	 * The default implementation always returns a null pointer.
	 * The start of the interval is CacheLine::SIZE aligned.
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
	virtual byte peekMem(word address, EmuTime::param time) const;

	/** Global writes.
	  * Some devices violate the MSX standard by ignoring the SLOT-SELECT
	  * signal; they react to writes to a certain address in _any_ slot.
	  * Luckily the only known example so far is 'Super Lode Runner'.
	  * This method is triggered for such 'global' writes.
	  * You need to register each address for which you want this method
	  * to be triggered.
	  */
	virtual void globalWrite(word address, byte value, EmuTime::param time);

	/** Invalidate CPU memory-mapping cache.
	  * This is a shortcut to the MSXCPU::invalidateMemCache() method,
	  * see that method for more details.
	  */
	void invalidateMemCache(word start, unsigned size);

	/** Get the mother board this device belongs to
	  */
	MSXMotherBoard& getMotherBoard() const;

	/** Get the configuration section for this device.
	  * This was passed as a constructor argument.
	  */
	const XMLElement& getDeviceConfig() const {
		return *deviceConfig.getXML();
	}
	const DeviceConfig& getDeviceConfig2() const { // TODO
		return deviceConfig;
	}

	/** Get the device references that are specified for this device
	 */
	const Devices& getReferences() const;

	// convenience functions, these delegate to MSXMotherBoard
	EmuTime::param getCurrentTime() const;
	MSXCPU& getCPU() const;
	MSXCPUInterface& getCPUInterface() const;
	Scheduler& getScheduler() const;
	CliComm& getCliComm() const;
	Reactor& getReactor() const;
	CommandController& getCommandController() const;
	PluggingController& getPluggingController() const;
	LedStatus& getLedStatus() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	/** Every MSXDevice has a config entry; this constructor gets
	  * some device properties from that config entry.
	  * @param config config entry for this device.
	  * @param name The name for the MSXDevice (will be made unique)
	  */
	MSXDevice(const DeviceConfig& config, const std::string& name);
	explicit MSXDevice(const DeviceConfig& config);

	/** Constructing a MSXDevice is a 2-step process, after the constructor
	  * is called this init() method must be called. The reason is exception
	  * safety (init() might throw and we use the destructor to clean up
	  * some stuff, this is more difficult when everything is done in the
	  * constrcutor).
	  * This is also a non-public method. This means you can only construct
	  * MSXDevices via DeviceFactory.
	  * In rare cases you need to override this method, for example when you
	  * need to access the referenced devices already during construction
	  * of this device (e.g. ADVram)
	  */
	friend class DeviceFactory;
	virtual void init();

	/** @see getDeviceInfo()
	 * Default implementation does nothing. Subclasses can override this
	 * method to add extra info (like subtypes).
	 */
	virtual void getExtraDeviceInfo(TclObject& result) const;

public:
	// public to allow non-MSXDevices to use these same arrays
	static byte unmappedRead [0x10000]; // Read only
	static byte unmappedWrite[0x10000]; // Write only

private:
	void initName(const std::string& name);
	void staticInit();

	void lockDevices();
	void unlockDevices();

	void registerSlots();
	void unregisterSlots();

	void registerPorts();
	void unregisterPorts();

	using MemRegions = std::vector<std::pair<unsigned, unsigned>>;
	MemRegions memRegions;
	std::vector<byte> inPorts;
	std::vector<byte> outPorts;

	DeviceConfig deviceConfig;
	std::string deviceName;

	Devices references;
	Devices referencedBy;

	int ps;
	int ss;
};

REGISTER_BASE_NAME_HELPER(MSXDevice, "Device");

#define REGISTER_MSXDEVICE(CLASS, NAME) \
REGISTER_POLYMORPHIC_INITIALIZER(MSXDevice, CLASS, NAME);

} // namespace openmsx

#endif
