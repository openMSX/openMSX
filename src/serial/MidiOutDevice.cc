// $Id$

#include "MidiOutDevice.hh"

using std::string;

namespace openmsx {

MidiOutDevice::~MidiOutDevice()
{
}

const string& MidiOutDevice::getClass() const
{
	static const string className("midi out");
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
