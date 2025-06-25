#include "DummyY8950KeyboardDevice.hh"

namespace openmsx {

void DummyY8950KeyboardDevice::write(uint8_t /*data*/, EmuTime /*time*/)
{
	// ignore data
}

uint8_t DummyY8950KeyboardDevice::read(EmuTime /*time*/)
{
	return 255;
}

std::string_view DummyY8950KeyboardDevice::getDescription() const
{
	return {};
}

void DummyY8950KeyboardDevice::plugHelper(Connector& /*connector*/,
                                          EmuTime /*time*/)
{
}

void DummyY8950KeyboardDevice::unplugHelper(EmuTime /*time*/)
{
}

} // namespace openmsx
