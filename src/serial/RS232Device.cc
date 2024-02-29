#include "RS232Device.hh"

namespace openmsx {

std::string_view RS232Device::getClass() const
{
	return "RS232";
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

bool RS232Device::getCTS(EmuTime::param /*time*/) const
{
	return true; // TODO check
}

bool RS232Device::getDSR(EmuTime::param /*time*/) const
{
	return true; // TODO check
}

bool RS232Device::getDCD(EmuTime::param /*time*/) const
{
	return true; // TODO check
}

bool RS232Device::getRI(EmuTime::param /*time*/) const
{
	return true; // TODO check
}

void RS232Device::setDTR(bool /*status*/, EmuTime::param /*time*/)
{
	// ignore
}

void RS232Device::setRTS(bool /*status*/, EmuTime::param /*time*/)
{
	// ignore
}

} // namespace openmsx
