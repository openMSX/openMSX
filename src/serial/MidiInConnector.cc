#include "MidiInConnector.hh"
#include "MidiInDevice.hh"
#include "DummyMidiInDevice.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "memory.hh"

namespace openmsx {

MidiInConnector::MidiInConnector(PluggingController& pluggingController_,
                                 string_ref name_)
	: Connector(pluggingController_, name_,
	            make_unique<DummyMidiInDevice>())
{
}

MidiInConnector::~MidiInConnector()
{
}

const std::string MidiInConnector::getDescription() const
{
	return "MIDI-in connector";
}

string_ref MidiInConnector::getClass() const
{
	return "midi in";
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
