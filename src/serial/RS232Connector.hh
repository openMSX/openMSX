// $Id: 

#ifndef __RS232CONNECTOR_HH__
#define __RS232CONNECTOR_HH__

#include "Connector.hh"
#include "SerialDataInterface.hh"

class DummyRS232Device;

class RS232Connector : public Connector, public SerialDataInterface
{
	public:
		RS232Connector(const string& name, const EmuTime& time);
		virtual ~RS232Connector();
		
		// Connector 
		virtual const string& getName() const;
		virtual const string& getClass() const;
		virtual void plug(Pluggable* device, const EmuTime &time);
		virtual void unplug(const EmuTime& time);
		
		// input (SerialDataInterface)
		virtual void setDataBits(DataBits bits) = 0;
		virtual void setStopBits(StopBits bits) = 0;
		virtual void setParityBit(bool enable, ParityBit parity) = 0;
		virtual void recvByte(byte value, const EmuTime& time) = 0;
		virtual bool ready() = 0;
		virtual bool acceptsData() = 0;
		
	private:
		string name;
		DummyRS232Device* dummy;
};

#endif
