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
	                 std::string name);

	MidiOutDevice& getPluggedMidiOutDev() const;

	// Connector
	std::string_view getDescription() const final override;
	std::string_view getClass() const final override;

	// SerialDataInterface
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, ParityBit parity) override;
	void recvByte(byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
