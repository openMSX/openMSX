// $Id$

#ifndef __MSXDEVICE_HH__
#define __MSXDEVICE_HH__

#include <string>

using std::string;

namespace openmsx {

class EmuTime;
class Device;


/** An MSXDevice is an emulated hardware component connected to the bus
  * of the emulated MSX. There is no communication among devices, only
  * between devices and the CPU.
  * MSXMemDevice contains the interface for memory mapped communication.
  * MSXIODevice contains the interface for I/O mapped communication.
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
	 * This method should reinitialize the device to the power-on state.
	 * The default implementation calls reset(). Only devices that act
	 * differently on a reset and a power-down / power-up cycle should
	 * reimplement this method.
	 * @param time The moment in time the power down-up occurs.
	 */
	virtual void reInit(const EmuTime& time);

	/**
	 * Returns a human-readable name for this device. The name is set
	 * in the setConfigDevice() method. This method is mostly used to
	 * print debug info.
	 * Default implementation is normally ok.
	 */
	virtual const string& getName() const;

protected:
	/** Every MSXDevice has a config entry; this constructor gets
	  * some device properties from that config entry.
	  * All subclasses must call this super-constructor.
	  * @param config config entry for this device.
	  * @param time the moment in emulated time this MSXDevice is
	  *   created (typically at time zero: power-up).
	  */
	MSXDevice(Device* config, const EmuTime& time);

	Device* deviceConfig;
	friend class VDPIODelay;
};

} // namespace openmsx

#endif //__MSXDEVICE_HH__

