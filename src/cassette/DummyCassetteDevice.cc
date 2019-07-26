#include "DummyCassetteDevice.hh"

namespace openmsx {

void DummyCassetteDevice::setMotor(bool /*status*/, EmuTime::param /*time*/)
{
	// do nothing
}

int16_t DummyCassetteDevice::readSample(EmuTime::param /*time*/)
{
	return 32767; // TODO check value
}

void DummyCassetteDevice::setSignal(bool /*output*/, EmuTime::param /*time*/)
{
	// do nothing
}

std::string_view DummyCassetteDevice::getDescription() const
{
	return {};
}

void DummyCassetteDevice::plugHelper(Connector& /*connector*/,
                                     EmuTime::param /*time*/)
{
}

void DummyCassetteDevice::unplugHelper(EmuTime::param /*time*/)
{
}

} // namespace openmsx
