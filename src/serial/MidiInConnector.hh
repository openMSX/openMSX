// $Id$

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
	MidiInConnector(PluggingController& pluggingController,
	                const std::string& name);
	virtual ~MidiInConnector();

	MidiInDevice& getPluggedMidiInDev() const;

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;

	virtual bool ready() = 0;
	virtual bool acceptsData() = 0;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

REGISTER_BASE_CLASS(MidiInConnector, "inConnector");

} // namespace openmsx

#endif
