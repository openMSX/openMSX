// $Id$

#include "RS232Device.hh"

using std::string;

namespace openmsx {

const string& RS232Device::getClass() const
{
	static const string className("RS232");
	return className;
}

void RS232Device::setDataBits(DataBits /*bits*/)
{
	// ignore
}

void RS232Device::setStopBits(StopBits /*bits*/)
{
	// ignore
}

void RS232Device::setParityBit(bool /*enable*/, ParityBit /*parity*/)
{
	// ignore
}

bool RS232Device::getCTS(const EmuTime& /*time*/) const
{
	return true;	// TODO check
}

bool RS232Device::getDSR(const EmuTime& /*time*/) const
{
	return true;	// TODO check
}

void RS232Device::setDTR(bool /*status*/, const EmuTime& /*time*/)
{
	// ignore
}

void RS232Device::setRTS(bool /*status*/, const EmuTime& /*time*/)
{
	// ignore
}

} // namespace openmsx
