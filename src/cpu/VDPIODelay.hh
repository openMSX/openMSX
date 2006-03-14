// $Id$

#ifndef VDPIODELAY_HH
#define VDPIODELAY_HH

#include "MSXDevice.hh"
#include "Clock.hh"

namespace openmsx {

class MSXCPU;

class VDPIODelay : public MSXDevice
{
public:
	VDPIODelay(MSXMotherBoard& motherboard, const XMLElement& config,
	           const EmuTime& time);

	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

	const MSXDevice& getInDevice(byte port) const;
	MSXDevice*& getInDevicePtr (byte port);
	MSXDevice*& getOutDevicePtr(byte port);

private:
	void delay(const EmuTime& time);

	MSXCPU& cpu;
	MSXDevice* inDevices[4];
	MSXDevice* outDevices[4];
	/** Remembers the time at which last VDP I/O action took place. */
	Clock<7159090> lastTime;
};

} // namespace openmsx

#endif
