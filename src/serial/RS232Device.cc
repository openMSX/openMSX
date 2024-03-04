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

// The input lines of the RS-232 port without any plugged in device
// are left floating (bits read as 1 in the IO ports), with the exception
// of the following devices:
//
// Spectravideo SVI-738 : Inputs are pull-up (Read as 0 in the IO ports)
// Spectravideo SVI-737 and SVI-757: Inputs are pull-down, behave
//                                   the same as left floating.
//
// This has been tested to be true for the SVI-738 and Toshiba HX-22,
// for the other devices the info comes from manuals, schematics, or
// circuit board pictures.
// 
// Return an empty optional to indicate an unplugged input, in which case
// the configuration tag 'rs232_pullup' will be used as the
// effective value.


std::optional<bool> RS232Device::getCTS(EmuTime::param /*time*/) const
{
	return {};
}

std::optional<bool> RS232Device::getDSR(EmuTime::param /*time*/) const
{
	return {};
}

std::optional<bool> RS232Device::getDCD(EmuTime::param /*time*/) const
{
	return {};
}

std::optional<bool> RS232Device::getRI(EmuTime::param /*time*/) const
{
	return {};
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
