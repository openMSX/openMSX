#ifndef MIDIOUTCONNECTOR_HH
#define MIDIOUTCONNECTOR_HH

#include "Connector.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class MidiOutDevice;

class MidiOutConnector final : public Connector, public SerialDataInterface
{
public:
	MidiOutConnector(PluggingController& pluggingController,
	                 string_ref name);
	virtual ~MidiOutConnector();

	MidiOutDevice& getPluggedMidiOutDev() const;

	// Connector
	virtual const std::string getDescription() const final;
	virtual string_ref getClass() const final;

	// SerialDataInterface
	virtual void setDataBits(DataBits bits);
	virtual void setStopBits(StopBits bits);
	virtual void setParityBit(bool enable, ParityBit parity);
	virtual void recvByte(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
