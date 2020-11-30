#ifndef MSXSWITCHEDDEVICE_HH
#define MSXSWITCHEDDEVICE_HH

#include "EmuTime.hh"
#include "openmsx.hh"

namespace openmsx {

class MSXMotherBoard;

class MSXSwitchedDevice
{
public:
	[[nodiscard]] virtual byte readSwitchedIO(word port, EmuTime::param time) = 0;
	[[nodiscard]] virtual byte peekSwitchedIO(word port, EmuTime::param time) const = 0;
	virtual void writeSwitchedIO(word port, byte value, EmuTime::param time) = 0;

protected:
	MSXSwitchedDevice(MSXMotherBoard& motherBoard, byte id);
	~MSXSwitchedDevice();

private:
	MSXMotherBoard& motherBoard;
	const byte id;
};

} // namespace openmsx

#endif
