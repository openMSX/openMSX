// $Id$

#ifndef VDPIODELAY_HH
#define VDPIODELAY_HH

#include "MSXDevice.hh"
#include "Clock.hh"

namespace openmsx {

class MSXCPU;
class MSXCPUInterface;

class VDPIODelay : public MSXDevice
{
public:
	VDPIODelay(MSXMotherBoard& motherboard, const DeviceConfig& config,
	           MSXCPUInterface& cpuInterface);

	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	const MSXDevice& getInDevice(byte port) const;
	MSXDevice*& getInDevicePtr (byte port);
	MSXDevice*& getOutDevicePtr(byte port);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void delay(EmuTime::param time);

	MSXCPU& cpu;
	MSXDevice* inDevices[4];
	MSXDevice* outDevices[4];
	/** Remembers the time at which last VDP I/O action took place. */
	Clock<7159090> lastTime;
};

} // namespace openmsx

#endif
