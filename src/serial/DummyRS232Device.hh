// $Id$

#ifndef __DUMMYRS232DEVICE_HH__
#define __DUMMYRS232DEVICE_HH__

#include "RS232Device.hh"


class DummyRS232Device : public RS232Device
{
	public:
		virtual void signal(const EmuTime& time);
		virtual void plug(Connector* connector, const EmuTime& time);
		virtual void unplug(const EmuTime& time);
		
		// SerialDataInterface (part)
		virtual void recvByte(byte value, const EmuTime& time);
};

#endif
