// $Id$

#ifndef __VDPIODELAY_HH__
#define __VDPIODELAY_HH__

#include "MSXIODevice.hh"
#include "EmuTime.hh"

namespace openmsx {

class MSXCPU;

class VDPIODelay : public MSXIODevice
{
public:
	VDPIODelay(MSXIODevice& device, const EmuTime& time);

	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

	const MSXIODevice& getDevice() const;

private:
	static const XMLElement& getConfig();
	void delay(const EmuTime& time);

	MSXCPU& cpu;
	MSXIODevice& device;
	/** Remembers the time at which last VDP I/O action took place. */
	Clock<7159090> lastTime;
};

} // namespace openmsx

#endif
