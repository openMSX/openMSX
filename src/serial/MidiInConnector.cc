// $Id$

#include "MidiInConnector.hh"
#include "MidiInDevice.hh"
#include "DummyMidiInDevice.hh"
#include "PluggingController.hh"


namespace openmsx {

MidiInConnector::MidiInConnector(const string &name)
	: Connector(name, new DummyMidiInDevice())
{
	PluggingController::instance()->registerConnector(this);
}

MidiInConnector::~MidiInConnector()
{
	PluggingController::instance()->unregisterConnector(this);
}

const string& MidiInConnector::getClass() const
{
	static const string className("midi in");
	return className;
}

} // namespace openmsx

