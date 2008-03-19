// $Id$

#include "MidiOutConnector.hh"
#include "MidiOutDevice.hh"
#include "DummyMidiOutDevice.hh"
#include "PluggingController.hh"
#include "checked_cast.hh"

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

void MidiOutConnector::recvByte(byte value, const EmuTime& time)
{
	getPluggedMidiOutDev().recvByte(value, time);
}

} // namespace openmsx
