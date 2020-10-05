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
	MidiInDevice& getPluggedMidiInDev() const;

	// Connector
	std::string_view getDescription() const final override;
	std::string_view getClass() const final override;

	virtual bool ready() = 0;
	virtual bool acceptsData() = 0;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	MidiInConnector(PluggingController& pluggingController,
	                std::string name);
	~MidiInConnector() = default;
};

REGISTER_BASE_CLASS(MidiInConnector, "inConnector");

} // namespace openmsx

#endif
