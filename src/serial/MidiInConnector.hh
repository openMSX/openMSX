// $Id$

#ifndef __MIDIINCONNECTOR_HH__
#define __MIDIINCONNECTOR_HH__

#include "Connector.hh"
#include "SerialDataInterface.hh"

namespace openmsx {

class DummyMidiInDevice;
class MidiInReader;

class MidiInConnector : public Connector, public SerialDataInterface
{
	public:
		MidiInConnector(const string& name, const EmuTime& time);
		virtual ~MidiInConnector();
		
		// Connector 
		virtual const string& getName() const;
		virtual const string& getClass() const;
		virtual void plug(Pluggable* device, const EmuTime &time);
		virtual void unplug(const EmuTime& time);

		virtual bool ready() = 0;
		virtual bool acceptsData() = 0;

	private:
		string name;
		DummyMidiInDevice* dummy;

		MidiInReader* reader;
};

} // namespace openmsx

#endif
