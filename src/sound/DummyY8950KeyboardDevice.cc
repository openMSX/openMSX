// $Id$

#include "DummyY8950KeyboardDevice.hh"

namespace openmsx {

void DummyY8950KeyboardDevice::write(byte /*data*/, EmuTime::param /*time*/)
{
	// ignore data
}

byte DummyY8950KeyboardDevice::read(EmuTime::param /*time*/)
{
	return 255;
}

const std::string& DummyY8950KeyboardDevice::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyY8950KeyboardDevice::plugHelper(Connector& /*connector*/,
                                          EmuTime::param /*time*/)
{
}

void DummyY8950KeyboardDevice::unplugHelper(EmuTime::param /*time*/)
{
}

} // namespace openmsx
