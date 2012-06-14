// $Id$

#ifndef DUMMYCASSETTEDEVICE_HH
#define DUMMYCASSETTEDEVICE_HH

#include "CassetteDevice.hh"

namespace openmsx {

class DummyCassetteDevice : public CassetteDevice
{
public:
	virtual void setMotor(bool status, EmuTime::param time);
	virtual void setSignal(bool output, EmuTime::param time);
	virtual short readSample(EmuTime::param time);

	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
};

} // namespace openmsx

#endif
