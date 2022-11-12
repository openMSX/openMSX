#include "DummyPrinterPortDevice.hh"

namespace openmsx {

bool DummyPrinterPortDevice::getStatus(EmuTime::param /*time*/)
{
	return true; // true = high = not ready
}

void DummyPrinterPortDevice::setStrobe(bool /*strobe*/, EmuTime::param /*time*/)
{
	// ignore strobe
}

void DummyPrinterPortDevice::writeData(uint8_t /*data*/, EmuTime::param /*time*/)
{
	// ignore data
}

std::string_view DummyPrinterPortDevice::getDescription() const
{
	return {};
}

void DummyPrinterPortDevice::plugHelper(
	Connector& /*connector*/, EmuTime::param /*time*/)
{
}

void DummyPrinterPortDevice::unplugHelper(EmuTime::param /*time*/)
{
}

} // namespace openmsx
