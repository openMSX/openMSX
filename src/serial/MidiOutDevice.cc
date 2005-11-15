// $Id$

#include "MidiOutDevice.hh"

namespace openmsx {

MidiOutDevice::~MidiOutDevice()
{
}

const std::string& MidiOutDevice::getClass() const
{
	static const std::string className("midi out");
	return className;
}


void MidiOutDevice::setDataBits(DataBits /*bits*/)
{
	// ignore
}

void MidiOutDevice::setStopBits(StopBits /*bits*/)
{
	// ignore
}

void MidiOutDevice::setParityBit(bool /*enable*/, ParityBit /*parity*/)
{
	// ignore
}

} // namespace openmsx
