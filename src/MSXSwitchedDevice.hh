// $Id$

#ifndef MSXSWITCHEDDEVICE_HH
#define MSXSWITCHEDDEVICE_HH

#include "noncopyable.hh"
#include "openmsx.hh"

namespace openmsx {

class MSXMotherBoard;
class EmuTime;

class MSXSwitchedDevice : private noncopyable
{
public:
	virtual byte readSwitchedIO(word port, const EmuTime& time) = 0;
	virtual byte peekSwitchedIO(word port, const EmuTime& time) const = 0;
	virtual void writeSwitchedIO(word port, byte value, const EmuTime& time) = 0;

protected:
	MSXSwitchedDevice(MSXMotherBoard& motherBoard, byte id);
	virtual ~MSXSwitchedDevice();

private:
	MSXMotherBoard& motherBoard;
	byte id;
};

} // namespace openmsx

#endif
