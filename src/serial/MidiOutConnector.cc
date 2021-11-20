#include "MidiOutConnector.hh"
#include "MidiOutDevice.hh"
#include "DummyMidiOutDevice.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

MidiOutConnector::MidiOutConnector(PluggingController& pluggingController_,
                                   std::string name_)
	: Connector(pluggingController_, std::move(name_),
	            std::make_unique<DummyMidiOutDevice>())
{
}

std::string_view MidiOutConnector::getDescription() const
{
	return "MIDI-out connector";
}

std::string_view MidiOutConnector::getClass() const
{
	return "midi out";
}

MidiOutDevice& MidiOutConnector::getPluggedMidiOutDev() const
{
	return *checked_cast<MidiOutDevice*>(&getPlugged());
}

void MidiOutConnector::setDataBits(DataBits bits)
{
	getPluggedMidiOutDev().setDataBits(bits);
}

void MidiOutConnector::setStopBits(StopBits bits)
{
	getPluggedMidiOutDev().setStopBits(bits);
}

void MidiOutConnector::setParityBit(bool enable, ParityBit parity)
{
	getPluggedMidiOutDev().setParityBit(enable, parity);
}

void MidiOutConnector::recvByte(byte value, EmuTime::param time)
{
	getPluggedMidiOutDev().recvByte(value, time);
}

template<typename Archive>
void MidiOutConnector::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Connector>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutConnector);

} // namespace openmsx
