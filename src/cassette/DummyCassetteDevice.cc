#include "DummyCassetteDevice.hh"

namespace openmsx {

void DummyCassetteDevice::setMotor(bool /*status*/, EmuTime::param /*time*/)
{
	// do nothing
}

bool DummyCassetteDevice::cassetteIn(EmuTime::param /*time*/)
{
	return true;
}

void DummyCassetteDevice::setSignal(bool /*output*/, EmuTime::param /*time*/)
{
	// do nothing
}

string_ref DummyCassetteDevice::getDescription() const
{
	return "";
}

void DummyCassetteDevice::plugHelper(Connector& /*connector*/,
                                     EmuTime::param /*time*/)
{
}

void DummyCassetteDevice::unplugHelper(EmuTime::param /*time*/)
{
}

} // namespace openmsx
