// $Id$

#ifndef __DummyIDEDevice_HH__
#define __DummyIDEDevice_HH__

#include "IDEDevice.hh"

namespace openmsx {

class DummyIDEDevice : public IDEDevice
{
public:
	virtual void reset(const EmuTime &time);

	virtual word readData(const EmuTime &time);
	virtual byte readReg(nibble reg, const EmuTime &time);

	virtual void writeData(word value, const EmuTime &time);
	virtual void writeReg(nibble reg, byte value, const EmuTime &time);
};

} // namespace openmsx
#endif
