// $Id$

#include "MidiOutDevice.hh"


const string& MidiOutDevice::getClass() const
{
	static const string className("Midi Out Port");
	return className;
}


void MidiOutDevice::setDataBits(DataBits bits)
{
	// ignore
}

void MidiOutDevice::setStopBits(StopBits bits)
{
	// ignore
}

void MidiOutDevice::setParityBits(bool enable, ParityBit parity)
{
	// ignore
}
