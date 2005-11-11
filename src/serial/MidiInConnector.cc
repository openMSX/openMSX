// $Id$

#include "MidiInConnector.hh"
#include "DummyMidiInDevice.hh"
#include "PluggingController.hh"

namespace openmsx {

MidiInConnector::MidiInConnector(PluggingController& pluggingController_,
                                 const std::string& name)
	: Connector(name, std::auto_ptr<Pluggable>(new DummyMidiInDevice()))
	, pluggingController(pluggingController_)
{
	pluggingController.registerConnector(*this);
}

MidiInConnector::~MidiInConnector()
{
	pluggingController.unregisterConnector(*this);
}

const std::string& MidiInConnector::getDescription() const
{
	static const std::string desc("Midi-IN connector.");
	return desc;
}

const std::string& MidiInConnector::getClass() const
{
	static const std::string className("midi in");
	return className;
}

MidiInDevice& MidiInConnector::getPlugged() const
{
	return static_cast<MidiInDevice&>(*plugged);
}

} // namespace openmsx

