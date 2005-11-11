// $Id$

#include "DummyCassetteDevice.hh"

namespace openmsx {

void DummyCassetteDevice::setMotor(bool /*status*/, const EmuTime& /*time*/)
{
	// do nothing
}

short DummyCassetteDevice::readSample(const EmuTime& /*time*/)
{
	return 32767;	// TODO check value
}

void DummyCassetteDevice::setSignal(bool /*output*/, const EmuTime& /*time*/)
{
	// do nothing
}

const std::string& DummyCassetteDevice::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyCassetteDevice::plugHelper(Connector& /*connector*/,
                                     const EmuTime& /*time*/)
{
}

void DummyCassetteDevice::unplugHelper(const EmuTime& /*time*/)
{
}

} // namespace openmsx
