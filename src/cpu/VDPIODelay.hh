// $Id$

#ifndef __VDPIODELAY_HH__
#define __VDPIODELAY_HH__

#include "MSXDevice.hh"
#include "EmuTime.hh"

namespace openmsx {

class MSXCPU;

class VDPIODelay : public MSXDevice
{
public:
	VDPIODelay(MSXDevice& device, const EmuTime& time);

	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

	const MSXDevice& getDevice() const;

private:
	static const XMLElement& getConfig();
	void delay(const EmuTime& time);

	MSXCPU& cpu;
	MSXDevice& device;
	/** Remembers the time at which last VDP I/O action took place. */
	Clock<7159090> lastTime;
};

} // namespace openmsx

#endif
