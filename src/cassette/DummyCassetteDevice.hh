// $Id$

#ifndef DUMMYCASSETTEDEVICE_HH
#define DUMMYCASSETTEDEVICE_HH

#include "CassetteDevice.hh"

namespace openmsx {

class DummyCassetteDevice : public CassetteDevice
{
public:
	virtual void setMotor(bool status, const EmuTime& time);
	virtual void setSignal(bool output, const EmuTime& time);
	virtual short readSample(const EmuTime& time);

	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
};

} // namespace openmsx

#endif
