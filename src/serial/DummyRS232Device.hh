// $Id$

#ifndef DUMMYRS232DEVICE_HH
#define DUMMYRS232DEVICE_HH

#include "RS232Device.hh"

namespace openmsx {

class DummyRS232Device : public RS232Device
{
public:
	virtual void signal(const EmuTime& time);
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// SerialDataInterface (part)
	virtual void recvByte(byte value, const EmuTime& time);
};

} // namespace openmsx

#endif
