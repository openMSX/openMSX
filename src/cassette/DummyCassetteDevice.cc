// $Id$

#include "DummyCassetteDevice.hh"


namespace openmsx {

DummyCassetteDevice::DummyCassetteDevice()
{
}

void DummyCassetteDevice::setMotor(bool status, const EmuTime &time)
{
	// do nothing
}

short DummyCassetteDevice::readSample(const EmuTime &time)
{
	return 32767;	// TODO check value
}

void DummyCassetteDevice::writeWave(short *buf, int length)
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

void DummyCassetteDevice::plug(Connector* connector, const EmuTime& time)
	throw()
{
}

void DummyCassetteDevice::unplug(const EmuTime& time)
{
}

} // namespace openmsx
