// $Id$

#include "MidiOutConnector.hh"
#include "MidiOutDevice.hh"
#include "DummyMidiOutDevice.hh"
#include "PluggingController.hh"


MidiOutConnector::MidiOutConnector(const string& name_, const EmuTime& time)
	: name(name_)
{
	dummy = new DummyMidiOutDevice();
	PluggingController::instance()->registerConnector(this);

	unplug(time);
}

MidiOutConnector::~MidiOutConnector()
{
	PluggingController::instance()->unregisterConnector(this);
	delete dummy;
}

const string& MidiOutConnector::getName() const
{
	return name;
}

const string& MidiOutConnector::getClass() const
{
	static const string className("Midi Out Port");
	return className;
}

void MidiOutConnector::plug(Pluggable* device, const EmuTime &time)
{
	Connector::plug(device, time);
}

void MidiOutConnector::unplug(const EmuTime& time)
{
	Connector::unplug(time);
	plug(dummy, time);
}

void MidiOutConnector::setDataBits(DataBits bits)
{
	((MidiOutDevice*)pluggable)->setDataBits(bits);
}

void MidiOutConnector::setStopBits(StopBits bits)
{
	((MidiOutDevice*)pluggable)->setStopBits(bits);
}

void MidiOutConnector::setParityBits(bool enable, ParityBit parity)
{
	((MidiOutDevice*)pluggable)->setParityBits(enable, parity);
}

void MidiOutConnector::recvByte(byte value, const EmuTime& time)
{
	((MidiOutDevice*)pluggable)->recvByte(value, time);
}
