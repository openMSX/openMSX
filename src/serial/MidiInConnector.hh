// $Id$

#ifndef MIDIINCONNECTOR_HH
#define MIDIINCONNECTOR_HH

#include "Connector.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class MidiInDevice;
class PluggingController;

class MidiInConnector : public Connector, public SerialDataInterface
{
public:
	MidiInConnector(PluggingController& pluggingController,
	                const std::string &name);
	virtual ~MidiInConnector();

	MidiInDevice& getPluggedMidiInDev() const;

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;

	virtual bool ready() = 0;
	virtual bool acceptsData() = 0;

private:
	PluggingController& pluggingController;
};

} // namespace openmsx

#endif
