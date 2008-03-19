// $Id$

#ifndef DUMMYY8950KEYBOARDDEVICE_HH
#define DUMMYY8950KEYBOARDDEVICE_HH

#include "Y8950KeyboardDevice.hh"

namespace openmsx {

class DummyY8950KeyboardDevice : public Y8950KeyboardDevice
{
public:
	virtual void write(byte data, const EmuTime& time);
	virtual byte read(const EmuTime& time);

	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
};

} // namespace openmsx

#endif
