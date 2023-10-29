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

	[[nodiscard]] MidiOutDevice& getPluggedMidiOutDev() const;

	// Connector
	[[nodiscard]] std::string_view getDescription() const final;
	[[nodiscard]] std::string_view getClass() const final;

	// SerialDataInterface
	void setDataBits(DataBits bits) override;
	void setStopBits(StopBits bits) override;
	void setParityBit(bool enable, ParityBit parity) override;
	void recvByte(byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
private:
	const std::string description;
};

} // namespace openmsx

#endif
