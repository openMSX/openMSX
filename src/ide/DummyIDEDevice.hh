// $Id$

#ifndef DUMMYIDEDEVICE_HH
#define DUMMYIDEDEVICE_HH

#include "IDEDevice.hh"
#include "serialize_meta.hh"

namespace openmsx {

class DummyIDEDevice : public IDEDevice
{
public:
	virtual void reset(const EmuTime& time);
	virtual word readData(const EmuTime& time);
	virtual byte readReg(nibble reg, const EmuTime& time);
	virtual void writeData(word value, const EmuTime& time);
	virtual void writeReg(nibble reg, byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
