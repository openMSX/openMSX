// $Id$

#include "MidiInConnector.hh"
#include "DummyMidiInDevice.hh"
#include "PluggingController.hh"

using std::string;

namespace openmsx {

MidiInConnector::MidiInConnector(const string &name)
	: Connector(name, std::auto_ptr<Pluggable>(new DummyMidiInDevice()))
{
	PluggingController::instance().registerConnector(this);
}

MidiInConnector::~MidiInConnector()
{
	PluggingController::instance().unregisterConnector(this);
}

const string& MidiInConnector::getDescription() const
{
	static const string desc("Midi-IN connector.");
	return desc;
}

const string& MidiInConnector::getClass() const
{
	static const string className("midi in");
	return className;
}

MidiInDevice& MidiInConnector::getPlugged() const
{
	return static_cast<MidiInDevice&>(*plugged);
}

} // namespace openmsx

