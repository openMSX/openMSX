#include "DummyPrinterPortDevice.hh"

namespace openmsx {

bool DummyPrinterPortDevice::getStatus(EmuTime /*time*/)
{
	return true; // true = high = not ready
}

void DummyPrinterPortDevice::setStrobe(bool /*strobe*/, EmuTime /*time*/)
{
	// ignore strobe
}

void DummyPrinterPortDevice::writeData(uint8_t /*data*/, EmuTime /*time*/)
{
	// ignore data
}

std::string_view DummyPrinterPortDevice::getDescription() const
{
	return {};
}

void DummyPrinterPortDevice::plugHelper(
	Connector& /*connector*/, EmuTime /*time*/)
{
}

void DummyPrinterPortDevice::unplugHelper(EmuTime /*time*/)
{
}

} // namespace openmsx
