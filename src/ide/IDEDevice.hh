// $Id$

#ifndef IDEDEVICE_HH
#define IDEDEVICE_HH

#include "openmsx.hh"

namespace openmsx {

class EmuTime;

class IDEDevice
{
public:
	virtual ~IDEDevice() {}
	virtual void reset(const EmuTime& time) = 0;

	virtual word readData(const EmuTime& time) = 0;
	virtual byte readReg(nibble reg, const EmuTime& time) = 0;

	virtual void writeData(word value, const EmuTime& time) = 0;
	virtual void writeReg(nibble reg, byte value, const EmuTime& time) = 0;
};

} // namespace openmsx

#endif
