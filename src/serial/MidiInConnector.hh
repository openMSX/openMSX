// $Id$

#ifndef __MIDIINCONNECTOR_HH__
#define __MIDIINCONNECTOR_HH__

#include "Connector.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class MidiInReader;


class MidiInConnector : public Connector, public SerialDataInterface {
public:
	MidiInConnector(const string &name);
	virtual ~MidiInConnector();

	// Connector
	virtual const string& getClass() const;

	virtual bool ready() = 0;
	virtual bool acceptsData() = 0;

private:
	MidiInReader* reader;
};

} // namespace openmsx

#endif // __MIDIINCONNECTOR_HH__
