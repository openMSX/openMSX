// $Id$

#ifndef __RS232CONNECTOR_HH__
#define __RS232CONNECTOR_HH__

#include "Connector.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class RS232Connector : public Connector, public SerialDataInterface {
public:
	RS232Connector(const string &name);
	virtual ~RS232Connector();

	// Connector
	virtual const string &getClass() const;

	// input (SerialDataInterface)
	virtual void setDataBits(DataBits bits) = 0;
	virtual void setStopBits(StopBits bits) = 0;
	virtual void setParityBit(bool enable, ParityBit parity) = 0;
	virtual void recvByte(byte value, const EmuTime& time) = 0;
	virtual bool ready() = 0;
	virtual bool acceptsData() = 0;
};

} // namespace openmsx

#endif
