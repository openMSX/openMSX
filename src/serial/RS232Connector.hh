// $Id$

#ifndef RS232CONNECTOR_HH
#define RS232CONNECTOR_HH

#include "Connector.hh"
#include "SerialDataInterface.hh"
#include "RS232Device.hh"

namespace openmsx {

class RS232Connector : public Connector, public SerialDataInterface
{
public:
	RS232Connector(const std::string& name);
	virtual ~RS232Connector();

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;
	virtual RS232Device& getPlugged() const;

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
