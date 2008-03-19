// $Id$

#ifndef MIDIOUTCONNECTOR_HH
#define MIDIOUTCONNECTOR_HH

#include "Connector.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class MidiOutDevice;
class PluggingController;

class MidiOutConnector : public Connector, public SerialDataInterface
{
public:
	MidiOutConnector(PluggingController& pluggingController,
	                 const std::string& name);
	virtual ~MidiOutConnector();

	MidiOutDevice& getPluggedMidiOutDev() const;

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;

	// SerialDataInterface
	virtual void setDataBits(DataBits bits);
	virtual void setStopBits(StopBits bits);
	virtual void setParityBit(bool enable, ParityBit parity);
	virtual void recvByte(byte value, const EmuTime& time);

private:
	PluggingController& pluggingController;
};

} // namespace openmsx

#endif
