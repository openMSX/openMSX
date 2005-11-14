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
	VDPIODelay(MSXMotherBoard& motherboard, MSXDevice& device,
	           const EmuTime& time);

	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

	const MSXDevice& getDevice() const;

private:
	void delay(const EmuTime& time);

	MSXCPU& cpu;
	MSXDevice& device;
	/** Remembers the time at which last VDP I/O action took place. */
	Clock<7159090> lastTime;
};

} // namespace openmsx

#endif
