// $Id$

#include "DummyCassetteDevice.hh"

using std::string;

namespace openmsx {

DummyCassetteDevice::DummyCassetteDevice()
{
}

void DummyCassetteDevice::setMotor(bool /*status*/, const EmuTime& /*time*/)
{
	// do nothing
}

short DummyCassetteDevice::readSample(const EmuTime& /*time*/)
{
	return 32767;	// TODO check value
}

void DummyCassetteDevice::writeWave(short* /*buf*/, int /*length*/)
{
	// do nothing
}
int DummyCassetteDevice::getWriteSampleRate()
{
	return 0;	// 0 means not interested
}

const string& DummyCassetteDevice::getDescription() const
{
	static const string EMPTY;
	return EMPTY;
}

void DummyCassetteDevice::plugHelper(Connector* /*connector*/,
                                     const EmuTime& /*time*/)
{
}

void DummyCassetteDevice::unplugHelper(const EmuTime& /*time*/)
{
}

} // namespace openmsx
