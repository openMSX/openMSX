// $Id$

#include "MidiOutDevice.hh"


const string& MidiOutDevice::getClass() const
{
	static const string className("midi out");
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
