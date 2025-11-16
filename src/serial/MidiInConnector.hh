#ifndef MIDIINCONNECTOR_HH
#define MIDIINCONNECTOR_HH

#include "Connector.hh"
#include "SerialDataInterface.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MidiInDevice;

class MidiInConnector : public Connector, public SerialDataInterface
{
public:
	[[nodiscard]] MidiInDevice& getPluggedMidiInDev() const;

	// Connector
	[[nodiscard]] zstring_view getDescription() const final;
	[[nodiscard]] zstring_view getClass() const final;

	[[nodiscard]] virtual bool ready() = 0;
	[[nodiscard]] virtual bool acceptsData() = 0;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	MidiInConnector(PluggingController& pluggingController,
	                std::string name);
	~MidiInConnector() = default;

private:
	const std::string description;
};

REGISTER_BASE_CLASS(MidiInConnector, "inConnector");

} // namespace openmsx

#endif
