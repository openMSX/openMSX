// $Id$

#include "DummyCassetteDevice.hh"
#include "EmuTime.hh"


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

const std::string &DummyCassetteDevice::getName()
{
	return name;
}
const std::string DummyCassetteDevice::name("dummy");
