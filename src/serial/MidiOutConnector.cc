// $Id$

#include "MidiOutConnector.hh"
#include "DummyMidiOutDevice.hh"
#include "PluggingController.hh"

namespace openmsx {

MidiOutConnector::MidiOutConnector(const string &name)
	: Connector(name, auto_ptr<Pluggable>(new DummyMidiOutDevice()))
{
	PluggingController::instance().registerConnector(this);
}

MidiOutConnector::~MidiOutConnector()
{
	PluggingController::instance().unregisterConnector(this);
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
