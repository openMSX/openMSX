#include "DummyCassetteDevice.hh"

namespace openmsx {

void DummyCassetteDevice::setMotor(bool /*status*/, EmuTime /*time*/)
{
	// do nothing
}

int16_t DummyCassetteDevice::readSample(EmuTime /*time*/)
{
	return 32767; // TODO check value
}

void DummyCassetteDevice::setSignal(bool /*output*/, EmuTime /*time*/)
{
	// do nothing
}

zstring_view DummyCassetteDevice::getDescription() const
{
	return {};
}

void DummyCassetteDevice::plugHelper(Connector& /*connector*/,
                                     EmuTime /*time*/)
{
}

void DummyCassetteDevice::unplugHelper(EmuTime /*time*/)
{
}

} // namespace openmsx
