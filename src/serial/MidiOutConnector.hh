// $Id$

#ifndef __MIDIOUTCONNECTOR_HH__
#define __MIDIOUTCONNECTOR_HH__

#include "Connector.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class MidiOutLogger;


class MidiOutConnector : public Connector, public SerialDataInterface {
public:
	MidiOutConnector(const string &name);
	virtual ~MidiOutConnector();

	// Connector
	virtual const string &getClass() const;

	// SerialDataInterface
	virtual void setDataBits(DataBits bits);
	virtual void setStopBits(StopBits bits);
	virtual void setParityBit(bool enable, ParityBit parity);
	virtual void recvByte(byte value, const EmuTime& time);

private:
	MidiOutLogger *logger;
};

} // namespace openmsx

#endif // __MIDIOUTCONNECTOR_HH__
