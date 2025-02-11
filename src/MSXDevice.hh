#ifndef MSXDEVICE_HH
#define MSXDEVICE_HH

#include "DeviceConfig.hh"
#include "EmuTime.hh"
#include "IterableBitSet.hh"
#include "narrow.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <array>
#include <cassert>
#include <span>
#include <string>
#include <vector>

namespace openmsx {

class XMLElement;
class MSXMotherBoard;
class MSXCPU;
class MSXCPUInterface;
class Scheduler;
class MSXCliComm;
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
	MSXDevice(MSXDevice&&) = delete;
	MSXDevice& operator=(const MSXDevice&) = delete;
	MSXDevice& operator=(MSXDevice&&) = delete;

	using Devices = std::vector<MSXDevice*>;

	virtual ~MSXDevice() = 0;

	/** Returns the hardwareconfig this device belongs to.
	  */
	[[nodiscard]] const HardwareConfig& getHardwareConfig() const {
		return deviceConfig.getHardwareConfig();
	}
	[[nodiscard]] HardwareConfig& getHardwareConfig() {
		return deviceConfig.getHardwareConfig();
	}

	/** Checks whether this device can be removed (no other device has a
	  * reference to it). Throws an exception if it can't be removed.
	  */
	void testRemove(std::span<const std::unique_ptr<MSXDevice>> removed) const;

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
	[[nodiscard]] virtual byte readIRQVector();

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
	[[nodiscard]] virtual const std::string& getName() const;

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
	[[nodiscard]] virtual byte readIO(word port, EmuTime::param time);

	/**
	 * Write a byte to a given IO port at a certain time to this
	 * device.
	 * The default implementation ignores the write (does nothing)
	 */
	virtual void writeIO(word port, byte value, EmuTime::param time);

	/**
	 * Read a byte from a given IO port. Reading via this method has no
	 * side effects (doesn't change the device status). If safe reading
	 * is not possible this method returns 0xFF.
	 * This method is not used by the emulation. It can however be used
	 * by a debugger.
	 * The default implementation just returns 0xFF.
	 */
	[[nodiscard]] virtual byte peekIO(word port, EmuTime::param time) const;


	// Memory

	/**
	 * Read a byte from a location at a certain time from this
	 * device.
	 * The default implementation returns 255.
	 */
	[[nodiscard]] virtual byte readMem(word address, EmuTime::param time);

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
	[[nodiscard]] virtual const byte* getReadCacheLine(word start) const;

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
	[[nodiscard]] virtual byte* getWriteCacheLine(word start);

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
	[[nodiscard]] virtual byte peekMem(word address, EmuTime::param time) const;

	/** Global writes.
	  * Some devices violate the MSX standard by ignoring the SLOT-SELECT
	  * signal; they react to writes to a certain address in _any_ slot.
	  * Luckily the only known example so far is 'Super Lode Runner'.
	  * This method is triggered for such 'global' writes.
	  * You need to register each address for which you want this method
	  * to be triggered.
	  */
	virtual void globalWrite(word address, byte value, EmuTime::param time);

	/** Global reads. Similar to globalWrite() but then for reads.
	  * 'Carnivore2' is an example of a device that monitors the MSX bus
	  * for reads in any slot.
	  */
	virtual void globalRead(word address, EmuTime::param time);

	/** Calls MSXCPUInterface::invalidateXXCache() for the specific (part
	  * of) the slot that this device is located in.
	  */
	void invalidateDeviceRWCache() { invalidateDeviceRWCache(0x0000, 0x10000); }
	void invalidateDeviceRCache()  { invalidateDeviceRCache (0x0000, 0x10000); }
	void invalidateDeviceWCache()  { invalidateDeviceWCache (0x0000, 0x10000); }
	void invalidateDeviceRWCache(unsigned start, unsigned size);
	void invalidateDeviceRCache (unsigned start, unsigned size);
	void invalidateDeviceWCache (unsigned start, unsigned size);

	/** Calls MSXCPUInterface::fillXXCache() for the specific (part of) the
	  * slot that this device is located in.
	  */
	void fillDeviceRWCache(unsigned start, unsigned size, byte* rwData);
	void fillDeviceRWCache(unsigned start, unsigned size, const byte* rData, byte* wData);
	void fillDeviceRCache (unsigned start, unsigned size, const byte* rData);
	void fillDeviceWCache (unsigned start, unsigned size, byte* wData);

	/** Get the mother board this device belongs to
	  */
	[[nodiscard]] MSXMotherBoard& getMotherBoard() const;

	/** Get the configuration section for this device.
	  * This was passed as a constructor argument.
	  */
	[[nodiscard]] const XMLElement& getDeviceConfig() const {
		return *deviceConfig.getXML();
	}
	[[nodiscard]] XMLElement& getDeviceConfig() {
		return *deviceConfig.getXML();
	}
	[[nodiscard]] const DeviceConfig& getDeviceConfig2() const { // TODO
		return deviceConfig;
	}
	[[nodiscard]] DeviceConfig& getDeviceConfig2() { // TODO
		return deviceConfig;
	}

	/** Get the device references that are specified for this device
	 */
	[[nodiscard]] const Devices& getReferences() const;

	// convenience functions, these delegate to MSXMotherBoard
	[[nodiscard]] EmuTime::param getCurrentTime() const;
	[[nodiscard]] MSXCPU& getCPU() const;
	[[nodiscard]] MSXCPUInterface& getCPUInterface() const;
	[[nodiscard]] Scheduler& getScheduler() const;
	[[nodiscard]] MSXCliComm& getCliComm() const;
	[[nodiscard]] Reactor& getReactor() const;
	[[nodiscard]] CommandController& getCommandController() const;
	[[nodiscard]] PluggingController& getPluggingController() const;
	[[nodiscard]] LedStatus& getLedStatus() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	/** Every MSXDevice has a config entry; this constructor gets
	  * some device properties from that config entry.
	  * @param config config entry for this device.
	  * @param name The name for the MSXDevice (will be made unique)
	  */
	MSXDevice(const DeviceConfig& config, std::string_view name);
	explicit MSXDevice(const DeviceConfig& config);

	/** Constructing a MSXDevice is a 2-step process, after the constructor
	  * is called this init() method must be called. The reason is exception
	  * safety (init() might throw and we use the destructor to clean up
	  * some stuff, this is more difficult when everything is done in the
	  * constructor).
	  * This is also a non-public method. This means you can only construct
	  * MSXDevices via DeviceFactory.
	  * In rare cases you need to override this method, for example when you
	  * need to access the referenced devices already during construction
	  * of this device (e.g. ADVram)
	  */
	friend class DeviceFactory;
	virtual void init();

	/** The 'base' and 'size' attribute values need to be at least aligned
	  * to CacheLine::SIZE. Though some devices may need a stricter
	  * alignment. In that case they must override this method.
	  */
	[[nodiscard]] virtual unsigned getBaseSizeAlignment() const;

	/** By default we don't allow unaligned <mem> specifications in the
	  * config file. Though for a machine like 'Victor HC-95A' is it useful
	  * to model it with combinations of unaligned devices. So we do allow
	  * it for a select few devices: devices that promise to not call any
	  * of the 'fillDeviceXXXCache()' methods.
	  */
	[[nodiscard]] virtual bool allowUnaligned() const { return false; }

	/** @see getDeviceInfo()
	 * Default implementation does nothing. Subclasses can override this
	 * method to add extra info (like subtypes).
	 */
	virtual void getExtraDeviceInfo(TclObject& result) const;

public:
	// public to allow non-MSXDevices to use these same arrays
	static inline std::array<byte, 0x10000> unmappedRead;  // Read only
	static inline std::array<byte, 0x10000> unmappedWrite; // Write only

private:
	template<typename Action, typename... Args>
	void clip(unsigned start, unsigned size, Action action, Args... args);

	void initName(std::string_view name);
	static void staticInit();

	void lockDevices();
	void unlockDevices();

	void registerSlots();
	void unregisterSlots();

	void registerPorts();
	void unregisterPorts();

protected:
	mutable std::string deviceName; // MSXMultiXxxDevice has a dynamic (lazy) name

	[[nodiscard]] byte getPrimarySlot() const {
		// must already be resolved to an actual slot
		assert((0 <= ps) && (ps <= 3));
		return narrow_cast<byte>(ps);
	}

private:
	struct BaseSize {
		unsigned base;
		unsigned size;
		[[nodiscard]] unsigned end() const { return base + size; }
	};
	using MemRegions = std::vector<BaseSize>;
	MemRegions memRegions;
	IterableBitSet<256> inPorts;
	IterableBitSet<256> outPorts;

	DeviceConfig deviceConfig;

	Devices references;
	Devices referencedBy;

	int ps = 0;
	int ss = 0;
};

REGISTER_BASE_NAME_HELPER(MSXDevice, "Device");

#define REGISTER_MSXDEVICE(CLASS, NAME) \
REGISTER_POLYMORPHIC_INITIALIZER(MSXDevice, CLASS, NAME);

} // namespace openmsx

#endif
