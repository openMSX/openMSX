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
	const std::string getDescription() const final override;
	string_ref getClass() const final override;

	virtual bool ready() = 0;
	virtual bool acceptsData() = 0;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	MidiInConnector(PluggingController& pluggingController,
	                string_ref name);
	~MidiInConnector();
};

REGISTER_BASE_CLASS(MidiInConnector, "inConnector");

} // namespace openmsx

#endif
