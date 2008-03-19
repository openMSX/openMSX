// $Id$

#include "DummyY8950KeyboardDevice.hh"

namespace openmsx {

void DummyY8950KeyboardDevice::write(byte /*data*/, const EmuTime& /*time*/)
{
	// ignore data
}

byte DummyY8950KeyboardDevice::read(const EmuTime& /*time*/)
{
	return 255;
}

const std::string& DummyY8950KeyboardDevice::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyY8950KeyboardDevice::plugHelper(Connector& /*connector*/,
                                          const EmuTime& /*time*/)
{
}

void DummyY8950KeyboardDevice::unplugHelper(const EmuTime& /*time*/)
{
}

} // namespace openmsx
