// $Id$

#include "DummyPrinterPortDevice.hh"

namespace openmsx {

bool DummyPrinterPortDevice::getStatus(const EmuTime& /*time*/)
{
	return true;	// true = high = not ready
}

void DummyPrinterPortDevice::setStrobe(bool /*strobe*/, const EmuTime& /*time*/)
{
	// ignore strobe
}

void DummyPrinterPortDevice::writeData(byte /*data*/, const EmuTime& /*time*/)
{
	// ignore data
}

const std::string& DummyPrinterPortDevice::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyPrinterPortDevice::plugHelper(
	Connector& /*connector*/, const EmuTime& /*time*/)
{
}

void DummyPrinterPortDevice::unplugHelper(const EmuTime& /*time*/)
{
}

} // namespace openmsx
