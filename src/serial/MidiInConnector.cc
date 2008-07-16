// $Id$

#include "MidiInConnector.hh"
#include "MidiInDevice.hh"
#include "DummyMidiInDevice.hh"
#include "checked_cast.hh"
#include "serialize.hh"

namespace openmsx {

MidiInConnector::MidiInConnector(PluggingController& pluggingController,
                                 const std::string& name)
	: Connector(pluggingController, name,
	            std::auto_ptr<Pluggable>(new DummyMidiInDevice()))
{
}

MidiInConnector::~MidiInConnector()
{
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

MidiInDevice& MidiInConnector::getPluggedMidiInDev() const
{
	return *checked_cast<MidiInDevice*>(&getPlugged());
}

template<typename Archive>
void MidiInConnector::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Connector>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(MidiInConnector);

} // namespace openmsx

