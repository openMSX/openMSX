// $Id$

#include "MidiOutConnector.hh"
#include "DummyMidiOutDevice.hh"
#include "PluggingController.hh"

using std::string;

namespace openmsx {

MidiOutConnector::MidiOutConnector(PluggingController& pluggingController_,
                                   const string &name)
	: Connector(name, std::auto_ptr<Pluggable>(new DummyMidiOutDevice()))
	, pluggingController(pluggingController_)
{
	pluggingController.registerConnector(*this);
}

MidiOutConnector::~MidiOutConnector()
{
	pluggingController.unregisterConnector(*this);
}

const string& MidiOutConnector::getDescription() const
{
	static const string desc("Midi-OUT connector.");
	return desc;
}

const string& MidiOutConnector::getClass() const
{
	static const string className("midi out");
	return className;
}

MidiOutDevice& MidiOutConnector::getPlugged() const
{
	return static_cast<MidiOutDevice&>(*plugged);
}

void MidiOutConnector::setDataBits(DataBits bits)
{
	getPlugged().setDataBits(bits);
}

void MidiOutConnector::setStopBits(StopBits bits)
{
	getPlugged().setStopBits(bits);
}

void MidiOutConnector::setParityBit(bool enable, ParityBit parity)
{
	getPlugged().setParityBit(enable, parity);
}

void MidiOutConnector::recvByte(byte value, const EmuTime& time)
{
	getPlugged().recvByte(value, time);
}

} // namespace openmsx
